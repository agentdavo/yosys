#include "kernel/yosys.h"
#include "kernel/sigtools.h"
#include "kernel/utils.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

// $add and $sub extend their inputs as signed only when both are signed.
static bool add_is_signed(Cell *add)
{
	return add->getParam(ID::A_SIGNED).as_bool() && add->getParam(ID::B_SIGNED).as_bool();
}

// The DSP adds exact products and exact partial sums, extended by the
// product signedness. An RTL operand gives the same low sum_width bits only
// if it keeps every bit below sum_width (or the whole exact value) and the
// adder extends it the same way. src is exact up to exact_width bits.
static bool exact_join(const SigSpec &src, int exact_width, bool src_signed,
		const SigSpec &port, bool port_signed, int sum_width)
{
	int w = GetSize(src);
	if (w == 0 || w < std::min(exact_width, sum_width))
		return false;
	SigSpec seen = port;
	seen.extend_u0(sum_width, port_signed);
	for (int i = 0; i < sum_width; i++) {
		SigBit want = i < w ? src[i] : (src_signed ? src[w - 1] : SigBit(State::S0));
		if (seen[i] != want)
			return false;
	}
	return true;
}

// A product feeding port p of add, checked at the final sum width.
static bool exact_product(const SigMap &sigmap, Cell *mul, Cell *add, IdString p, int sum_width)
{
	int exact_width = GetSize(mul->getPort(ID::A)) + GetSize(mul->getPort(ID::B));
	return exact_join(sigmap(mul->getPort(ID::Y)), exact_width, mul->getParam(ID::A_SIGNED).as_bool(),
			sigmap(add->getPort(p)), add_is_signed(add), sum_width);
}

// A partial sum from lo feeding port p of hi. Only a partial sum at least
// as wide as the final sum is known to be exact where it matters.
static bool exact_partial(const SigMap &sigmap, Cell *lo, Cell *hi, IdString p, int sum_width)
{
	return exact_join(sigmap(lo->getPort(ID::Y)), INT_MAX, false,
			sigmap(hi->getPort(p)), add_is_signed(hi), sum_width);
}

// Bits needed to hold s: drop constant-zero MSBs (unsigned) or MSBs that
// repeat the bit below (signed).
static int value_width(const SigMap &sigmap, SigSpec s, bool is_signed)
{
	s = sigmap(s);
	int w = GetSize(s);
	if (is_signed)
		while (w > 1 && s[w - 1] == s[w - 2])
			w--;
	else
		while (w > 0 && s[w - 1] == State::S0)
			w--;
	return w;
}

// A MAC's sum is bounded by its operand widths, so the Y bits above that
// bound are only an extension. Drive them as one in the fabric, so a later
// multiply by the sum sees its true width and fits a single DSP instead of
// being split with a LUT partial product.
static void bound_mac_outputs(Module *module)
{
	SigMap sigmap(module);
	for (auto cell : module->cells()) {
		bool is_signed;
		std::vector<std::pair<IdString, IdString>> products;
		IdString c_port;
		if (cell->type == ID($__NX_MAC9X9WIDE_4LANE)) {
			is_signed = cell->getParam(ID(SIGNED)).as_bool();
			products = {{ID(A0), ID(B0)}, {ID(A1), ID(B1)}, {ID(A2), ID(B2)}, {ID(A3), ID(B3)}};
			if (cell->getParam(ID(USE_C)).as_bool())
				c_port = ID::C;
		} else if (cell->type.in(ID($__NX_MAC18X18), ID($__NX_MAC36X36))) {
			is_signed = cell->getParam(ID::A_SIGNED).as_bool();
			// An unsigned C - A*B wraps below zero, so it has no bound.
			if (!is_signed && cell->getParam(ID(SUBTRACT)).as_bool())
				continue;
			products = {{ID::A, ID::B}};
			c_port = ID::C;
		} else {
			continue;
		}
		if (cell->hasParam(ID(ACCUM)) && cell->getParam(ID(ACCUM)).as_bool())
			continue;

		int terms = GetSize(products);
		int widest = 0;
		for (auto &p : products)
			widest = std::max(widest, value_width(sigmap, cell->getPort(p.first), is_signed) +
					value_width(sigmap, cell->getPort(p.second), is_signed));
		if (c_port != IdString()) {
			widest = std::max(widest, value_width(sigmap, cell->getPort(c_port), is_signed));
			terms++;
		}
		int exact = widest + ceil_log2(terms);

		SigSpec y = cell->getPort(ID::Y);
		if (exact <= 0 || exact >= GetSize(y))
			continue;
		SigSpec new_y = y.extract(0, exact);
		new_y.append(module->addWire(NEW_ID, GetSize(y) - exact));
		cell->setPort(ID::Y, new_y);
		for (int i = exact; i < GetSize(y); i++)
			module->connect(y[i], is_signed ? y[exact - 1] : SigBit(State::S0));
	}
}

// The value range of a product or sum follows from its operands' ranges.
// A $mul or $add declared wider than its range needs only repeats the sign
// (or zero) in its extra Y bits: tie them off, so a later multiply or shift
// of the result sees its true width. Ranges rather than widths, because a
// chain of sums grows by one bit per add in width but far less in range:
// four 16-bit products fit 18 bits, not 19. Ranges reaching +/-2^62 are left
// unbounded, so the arithmetic never overflows.
static pool<IdString> bound_arith(Module *module)
{
	typedef std::pair<int64_t, int64_t> Range;
	const int64_t lim = int64_t(1) << 62;
	auto width_range = [&](int w, bool s) -> Range {
		if (w >= 62)
			return {-lim, lim};
		if (!s)
			return {0, (int64_t(1) << w) - 1};
		return {-(int64_t(1) << (w - 1)), (int64_t(1) << (w - 1)) - 1};
	};
	// A range touching +/-2^62 is saturated: its width is unknown.
	auto saturated = [&](Range r) { return r.first <= -lim || r.second >= lim; };
	auto range_width = [&](Range r, bool s) {
		if (saturated(r))
			return INT_MAX;
		int w = 1;
		while (w < 63 && !(r.first >= width_range(w, s).first && r.second <= width_range(w, s).second))
			w++;
		return w;
	};
	auto sat_mul = [&](int64_t a, int64_t b) -> int64_t {
		long double p = (long double)a * (long double)b;
		if (p > (long double)lim)
			return lim;
		if (p < -(long double)lim)
			return -lim;
		return a * b;
	};

	SigMap sigmap(module);
	std::vector<Cell*> cells;
	dict<SigBit, Cell*> y0_of;
	for (auto cell : module->cells())
		if (cell->type.in(ID($mul), ID($add))) {
			cells.push_back(cell);
			y0_of[sigmap(cell->getPort(ID::Y))[0]] = cell;
		}
	// Low Y bits of each cell that still carry its value, and its range.
	dict<Cell*, int> live;
	dict<Cell*, Range> range;
	for (auto cell : cells)
		live[cell] = GetSize(cell->getPort(ID::Y));

	// The range of an operand read with signedness s: the producing cell's
	// range if the operand is exactly that value, else its width's range.
	auto operand = [&](const SigSpec &port, bool s) -> Range {
		SigSpec p = sigmap(port);
		int w = value_width(sigmap, p, s);
		if (GetSize(p) > 0 && y0_of.count(p[0])) {
			Cell *src = y0_of.at(p[0]);
			bool src_s = src->getParam(ID::A_SIGNED).as_bool() && src->getParam(ID::B_SIGNED).as_bool();
			// Only the whole value counts: all live Y bits, with anything
			// above them an extension. A truncated copy is a different value.
			int lw = live.at(src);
			if (range.count(src) && src_s == s && GetSize(p) >= lw && w <= lw &&
					p.extract(0, lw) == sigmap(src->getPort(ID::Y)).extract(0, lw))
				return range.at(src);
		}
		return width_range(w, s);
	};

	// Cells whose Y holds their exact result.
	dict<Cell*, bool> fits;
	bool changed = true;
	while (changed) {
		changed = false;
		for (auto cell : cells) {
			// $mul and $add extend both inputs as signed only if both are.
			bool s = cell->getParam(ID::A_SIGNED).as_bool() && cell->getParam(ID::B_SIGNED).as_bool();
			Range a = operand(cell->getPort(ID::A), s);
			Range b = operand(cell->getPort(ID::B), s);
			Range r;
			if (cell->type == ID($mul)) {
				int64_t c[4] = {sat_mul(a.first, b.first), sat_mul(a.first, b.second),
						sat_mul(a.second, b.first), sat_mul(a.second, b.second)};
				r = {*std::min_element(c, c + 4), *std::max_element(c, c + 4)};
			} else {
				if (saturated(a) || saturated(b))
					r = {-lim, lim};
				else
					r = {std::max(a.first + b.first, -lim), std::min(a.second + b.second, lim)};
			}
			int exact = range_width(r, s);
			fits[cell] = exact <= live.at(cell);
			// A result wider than Y wraps: Y then holds anything in its width.
			if (exact > live.at(cell))
				r = width_range(live.at(cell), s);
			if (!range.count(cell) || range.at(cell) != r) {
				range[cell] = r;
				changed = true;
			}
			if (exact >= live.at(cell))
				continue;
			SigSpec y = cell->getPort(ID::Y);
			cell->setPort(ID::Y, y.extract(0, exact));
			cell->setParam(ID::Y_WIDTH, exact);
			for (int i = exact; i < live.at(cell); i++) {
				SigBit e = s ? y[exact - 1] : SigBit(State::S0);
				module->connect(y[i], e);
				sigmap.add(y[i], e);
			}
			live[cell] = exact;
			changed = true;
		}
	}
	pool<IdString> exact_cells;
	for (auto &it : fits)
		if (it.second)
			exact_cells.insert(it.first->name);
	return exact_cells;
}

// Which DSP primitive an a x b multiply takes: 0 for 18x18, 1 for 36x18,
// 2 for 36x36, -1 when it is split over several (always on ECP5 past 18).
static int dsp_class(int a, int b, bool ecp5)
{
	if (a > b)
		std::swap(a, b);
	if (b <= 18)
		return 0;
	if (ecp5)
		return -1;
	if (a <= 18 && b <= 36)
		return 1;
	if (b <= 36)
		return 2;
	return -1;
}

// A block scale after an integer dot, (-1)^s * m * 2^e, arrives as
// s ? -((dot * m) << e) : ((dot * m) << e) on a wide product. The sign and
// the low bits of the shift are the same operation applied to the narrow
// operand m, where they cost a few LUTs instead of a wide negate and several
// barrel-shifter levels after the DSP. Move them there as far as the
// operand can grow without changing which DSP primitive the multiply takes,
// or with wide_scale, as far as MULT18X36 when it would have been MULT18X18.
static void fold_scale_into_mul(Module *module, bool ecp5, bool wide_scale = false)
{
	// The primitive a folded multiply may end up on, given the one it has.
	auto fits = [&](int a, int b, int cls) {
		int c = dsp_class(a, b, ecp5);
		return c == cls || (wide_scale && !ecp5 && cls == 0 && c == 1);
	};
	// A shift is folded once; its remainder's low bits are then zero.
	pool<Cell*> folded_shl;
	bool changed = true;
	while (changed) {
		changed = false;
		SigMap sigmap(module);
		dict<SigBit, Cell*> driver;
		dict<SigBit, pool<Cell*>> readers;
		pool<SigBit> out_bits;
		for (auto cell : module->cells())
			for (auto &conn : cell->connections()) {
				bool is_input = !cell->known() || cell->input(conn.first);
				for (auto bit : sigmap(conn.second)) {
					if (!bit.wire)
						continue;
					if (is_input)
						readers[bit].insert(cell);
					else
						driver[bit] = cell;
				}
			}
		// A kept wire is read from outside the netlist too, like an output.
		for (auto wire : module->wires())
			if (wire->port_output || wire->get_bool_attribute(ID::keep))
				for (auto bit : sigmap(SigSpec(wire)))
					out_bits.insert(bit);

		// The cells (nullptr: an output or kept wire) that read any bit of sig.
		auto users = [&](const SigSpec &sig) {
			pool<Cell*> u;
			for (auto bit : sigmap(sig)) {
				if (out_bits.count(bit))
					u.insert(nullptr);
				auto it = readers.find(bit);
				if (it != readers.end())
					u.insert(it->second.begin(), it->second.end());
			}
			return u;
		};
		// The cell driving exactly sig, if it is one cell's whole output.
		auto driven_by = [&](const SigSpec &sig, IdString type_a, IdString type_b = IdString()) -> Cell* {
			SigSpec s = sigmap(sig);
			if (GetSize(s) == 0 || !s[0].wire || !driver.count(s[0]))
				return nullptr;
			Cell *c = driver.at(s[0]);
			if (!c->type.in(type_a, type_b) || sigmap(c->getPort(ID::Y)) != s)
				return nullptr;
			return c;
		};
		// A multiply whose output holds the whole product, so shifting or
		// negating an operand is the same as doing it to the output.
		auto exact_mul = [&](Cell *m) {
			return m && GetSize(m->getPort(ID::Y)) >= GetSize(m->getPort(ID::A)) + GetSize(m->getPort(ID::B));
		};
		auto mul_signed = [&](Cell *m) {
			return m->getParam(ID::A_SIGNED).as_bool() && m->getParam(ID::B_SIGNED).as_bool();
		};
		// Replace operand p of m, and give m a fresh output holding the
		// whole new product (never narrower than before).
		auto set_operand = [&](Cell *m, IdString p, const SigSpec &v) {
			m->setPort(p, v);
			m->setParam(p == ID::A ? ID::A_WIDTH : ID::B_WIDTH, GetSize(v));
			int w = std::max(GetSize(m->getPort(ID::Y)), GetSize(m->getPort(ID::A)) + GetSize(m->getPort(ID::B)));
			SigSpec y = module->addWire(NEW_ID, w);
			m->setPort(ID::Y, y);
			m->setParam(ID::Y_WIDTH, w);
			return y;
		};

		pool<Cell*> done;
		std::vector<Cell*> cells(module->cells().begin(), module->cells().end());

		// Sign: mux(s, X, -X) with X a product, or a left shift of one.
		for (auto mux : cells) {
			if (mux->type != ID($mux) || done.count(mux))
				continue;
			bool neg_on_b = true;
			Cell *neg = driven_by(mux->getPort(ID::B), ID($neg));
			if (!neg) {
				neg = driven_by(mux->getPort(ID::A), ID($neg));
				neg_on_b = false;
			}
			if (!neg || done.count(neg))
				continue;
			// The mux may be wider than X (a wider accumulator): its other
			// input is then X extended the same way the $neg extends it.
			SigSpec x = sigmap(neg->getPort(ID::A));
			int w = GetSize(mux->getPort(ID::Y));
			bool x_signed = neg->getParam(ID::A_SIGNED).as_bool();
			SigSpec x_ext = x;
			x_ext.extend_u0(w, x_signed);
			if (GetSize(neg->getPort(ID::Y)) != w || sigmap(mux->getPort(neg_on_b ? ID::A : ID::B)) != x_ext)
				continue;
			if (w > GetSize(x) && !x_signed)
				continue;
			Cell *shl = driven_by(x, ID($shl), ID($sshl));
			Cell *mul = shl ? driven_by(shl->getPort(ID::A), ID($mul)) : driven_by(x, ID($mul));
			if (!exact_mul(mul) || !mul_signed(mul) || done.count(mul) || (shl && done.count(shl)))
				continue;
			if (shl && (users(mul->getPort(ID::Y)) != pool<Cell*>{shl} || !shl->getParam(ID::A_SIGNED).as_bool()))
				continue;
			// Extended, -ext(X) is ext(-X) only if -X still fits X: the
			// shifted product must never reach X's most negative value.
			if (shl && w > GetSize(x) && GetSize(x) < GetSize(mul->getPort(ID::A)) + GetSize(mul->getPort(ID::B)) +
					(1 << std::min(GetSize(shl->getPort(ID::B)), 20)) - 1)
				continue;
			if (users(x) != pool<Cell*>{neg, mux})
				continue;

			// Negate the narrower operand. A non-negative one needs no
			// extra bit for its negation.
			IdString best;
			int best_w = 0;
			int wa = value_width(sigmap, mul->getPort(ID::A), true);
			int wb = value_width(sigmap, mul->getPort(ID::B), true);
			for (auto p : {ID::A, ID::B}) {
				SigSpec z = sigmap(mul->getPort(p));
				int wz = p == ID::A ? wa : wb, wo = p == ID::A ? wb : wa;
				int nw = wz + (z[wz - 1] == State::S0 ? 0 : 1);
				int cls = dsp_class(wa, wb, ecp5);
				if (cls < 0 || !fits(nw, wo, cls))
					continue;
				if (best == IdString() || nw < best_w)
					best = p, best_w = nw;
			}
			if (best == IdString())
				continue;
			SigSpec z = mul->getPort(best);
			z = z.extract(0, best == ID::A ? wa : wb);
			z.extend_u0(best_w, true);
			SigSpec nz = module->Neg(NEW_ID, z, true);
			SigSpec sel = mux->getPort(ID::S);
			SigSpec y = set_operand(mul, best, neg_on_b ? module->Mux(NEW_ID, z, nz, sel) : module->Mux(NEW_ID, nz, z, sel));
			SigSpec res;
			if (shl) {
				shl->setPort(ID::A, y);
				shl->setParam(ID::A_WIDTH, GetSize(y));
				res = shl->getPort(ID::Y);
				done.insert(shl);
			} else {
				res = y.extract(0, std::min(GetSize(y), GetSize(x)));
			}
			res.extend_u0(w, x_signed);
			module->connect(mux->getPort(ID::Y), res);
			log("  Folding sign of %s into operand %s of %s.\n", log_id(mux), log_id(best), log_id(mul));
			done.insert(mul);
			module->remove(neg);
			module->remove(mux);
			changed = true;
			// neg and mux are gone; rescan rather than walk past them.
			break;
		}
		if (changed)
			continue;

		// Shift: (A * B) << S becomes A * (Z << S_lo), then << the rest.
		for (auto shl : cells) {
			if (!shl->type.in(ID($shl), ID($sshl)) || shl->getParam(ID::B_SIGNED).as_bool() || folded_shl.count(shl))
				continue;
			Cell *mul = driven_by(shl->getPort(ID::A), ID($mul));
			if (!exact_mul(mul) || users(mul->getPort(ID::Y)) != pool<Cell*>{shl})
				continue;
			bool is_signed = mul_signed(mul);
			if (shl->getParam(ID::A_SIGNED).as_bool() != is_signed)
				continue;
			SigSpec s = shl->getPort(ID::B);
			int n = GetSize(s);
			int wa = value_width(sigmap, mul->getPort(ID::A), is_signed);
			int wb = value_width(sigmap, mul->getPort(ID::B), is_signed);
			int cls = dsp_class(wa, wb, ecp5);
			if (cls < 0)
				continue;
			IdString best;
			int best_k = 0;
			for (auto p : {ID::A, ID::B}) {
				int wz = p == ID::A ? wa : wb, wo = p == ID::A ? wb : wa;
				int k = 0;
				while (k < n && k < 6 && fits(wz + (1 << (k + 1)) - 1, wo, cls))
					k++;
				// On a tie shift the narrower operand: fewer mux bits, and
				// usually the scale, off the dot's critical path.
				int best_w = best == ID::A ? wa : wb;
				if (k > best_k || (k == best_k && k > 0 && wz < best_w))
					best = p, best_k = k;
			}
			if (best_k == 0)
				continue;
			SigSpec z = mul->getPort(best);
			z = z.extract(0, best == ID::A ? wa : wb);
			z.extend_u0(GetSize(z) + (1 << best_k) - 1, is_signed);
			SigSpec y = set_operand(mul, best, module->Shl(NEW_ID, z, s.extract(0, best_k), is_signed));
			log("  Folding %d low shift bits of %s into operand %s of %s.\n", best_k, log_id(shl), log_id(best), log_id(mul));
			if (best_k == n) {
				// Nothing left to shift: the product feeds the shift's
				// readers directly, so a following add can still fuse.
				y.extend_u0(GetSize(shl->getPort(ID::Y)), is_signed);
				module->connect(shl->getPort(ID::Y), y);
				module->remove(shl);
			} else {
				shl->setPort(ID::A, y);
				shl->setParam(ID::A_WIDTH, GetSize(y));
				// The remaining shift is S with its low best_k bits cleared.
				SigSpec rest(State::S0, best_k);
				rest.append(s.extract(best_k, n - best_k));
				shl->setPort(ID::B, rest);
				folded_shl.insert(shl);
			}
			changed = true;
			break;
		}
	}
}

// A variable left shift by a few bits is a multiply by a power of two. With
// -dsp-shift, X << S (S of 2 to 4 non-constant bits, X at most 18 bits wide)
// becomes X * (1 << S) on a DSP: the LUT barrel shifter, one mux level per
// bit of S across all of X, gives way to a 16-way decoder on the one-hot.
static void shift_to_mul(Module *module)
{
	SigMap sigmap(module);
	std::vector<Cell*> shifts;
	for (auto cell : module->cells())
		if (cell->type.in(ID($shl), ID($sshl)) && !cell->getParam(ID::B_SIGNED).as_bool())
			shifts.push_back(cell);
	for (auto shl : shifts) {
		bool is_signed = shl->getParam(ID::A_SIGNED).as_bool();
		SigSpec s = shl->getPort(ID::B);
		if (sigmap(s).is_fully_const())
			continue;
		int n = value_width(sigmap, s, false);
		if (n < 2 || n > 4)
			continue;
		SigSpec x = shl->getPort(ID::A);
		int wx = value_width(sigmap, x, is_signed);
		// A signed product needs a zero sign bit above the one-hot.
		int ow = (1 << n) + (is_signed ? 1 : 0);
		if (wx == 0 || wx > 18 || ow > 18)
			continue;

		SigSpec one(State::S0, ow);
		one[0] = State::S1;
		SigSpec onehot = module->Shl(NEW_ID, one, s.extract(0, n), false);
		SigSpec p = module->addWire(NEW_ID, wx + ow);
		module->addMul(NEW_ID, x.extract(0, wx), onehot, p, is_signed);
		// The product is exact, so fitting it to Y is the shift's own
		// extension or truncation of A.
		SigSpec y = shl->getPort(ID::Y);
		p.extend_u0(GetSize(y), is_signed);
		module->connect(y, p);
		log("  Mapping %d-bit shift %s of a %d-bit value onto a multiply.\n", n, log_id(shl), wx);
		module->remove(shl);
	}
}

// Drop the extension MSBs from $mul operands, so each multiply is matched
// and split by its true width.
static void narrow_mul_operands(Module *module)
{
	SigMap sigmap(module);
	for (auto cell : module->cells()) {
		if (cell->type != ID($mul))
			continue;
		bool is_signed = cell->getParam(ID::A_SIGNED).as_bool() && cell->getParam(ID::B_SIGNED).as_bool();
		for (auto p : {ID::A, ID::B}) {
			SigSpec s = cell->getPort(p);
			int w = std::max(value_width(sigmap, s, is_signed), 1);
			if (w >= GetSize(s))
				continue;
			cell->setPort(p, s.extract(0, w));
			cell->setParam(p == ID::A ? ID::A_WIDTH : ID::B_WIDTH, w);
		}
	}
}

// The clock and control set of one rising-edge flop the DSP input
// registers can copy: an optional enable and a sync reset to zero, with
// reset over enable.
struct RegCtrl {
	SigBit clk;
	SigBit en = State::S1;
	bool en_inv = false;
	SigBit rst = State::S0;
	bool rst_inv = false;
	bool operator==(const RegCtrl &o) const {
		return clk == o.clk && en == o.en && en_inv == o.en_inv && rst == o.rst && rst_inv == o.rst_inv;
	}
	bool operator!=(const RegCtrl &o) const { return !(*this == o); }
};

// The input registers of a 4-lane dot: lane i has one REGINPUTABi for both
// A_i and B_i. Lanes 0 and 1 share the A and the B clock enable and reset,
// as do lanes 2 and 3, and the whole cell has one clock. A lane whose
// operands come straight from flops gets a copy of those flops inside the
// DSP; opt_clean drops the fabric flops once nothing else reads them.
// Whether a flop can be one of the DSP's: rising edge, and a sync reset (if
// any) to zero, taking priority over the enable.
static bool dsp_like_ff(Cell *ff)
{
	if (!ff->type.in(ID($dff), ID($dffe), ID($sdff), ID($sdffe)))
		return false;
	if (!ff->getParam(ID::CLK_POLARITY).as_bool())
		return false;
	return !ff->type.in(ID($sdff), ID($sdffe)) || ff->getParam(ID::SRST_VALUE).is_fully_zero();
}

static RegCtrl ctrl_of(const SigMap &sigmap, Cell *ff)
{
	RegCtrl ctrl;
	ctrl.clk = sigmap(ff->getPort(ID::CLK)[0]);
	if (ff->type.in(ID($dffe), ID($sdffe))) {
		ctrl.en = sigmap(ff->getPort(ID::EN)[0]);
		ctrl.en_inv = !ff->getParam(ID::EN_POLARITY).as_bool();
	}
	if (ff->type.in(ID($sdff), ID($sdffe))) {
		ctrl.rst = sigmap(ff->getPort(ID::SRST)[0]);
		ctrl.rst_inv = !ff->getParam(ID::SRST_POLARITY).as_bool();
	}
	return ctrl;
}

// DSP registers power up and reset to zero, so a flop bit may move into one
// only if its initial value is zero or unknown.
static bool zero_or_no_init(SigBit qb)
{
	if (!qb.wire)
		return true;
	auto it = qb.wire->attributes.find(ID::init);
	if (it == qb.wire->attributes.end() || qb.offset >= GetSize(it->second))
		return true;
	State s = it->second[qb.offset];
	return s == State::Sx || s == State::S0;
}

// Every DSP-like flop bit in a module, by its Q.
struct FlopIndex {
	const SigMap &sigmap;
	dict<SigBit, std::pair<SigBit, RegCtrl>> q2d;

	FlopIndex(Module *module, const SigMap &sigmap) : sigmap(sigmap)
	{
		for (auto ff : module->cells()) {
			if (!dsp_like_ff(ff))
				continue;
			RegCtrl ctrl = ctrl_of(sigmap, ff);
			SigSpec q = ff->getPort(ID::Q);
			SigSpec d = ff->getPort(ID::D);
			for (int i = 0; i < GetSize(q); i++)
				if (zero_or_no_init(q[i]))
					q2d[sigmap(q[i])] = {d[i], ctrl};
		}
	}

	// Map one operand through its flops: every non-constant bit must be a
	// flop Q with the same control set. Empty ctrl.clk means all constant.
	bool through(const SigSpec &s, SigSpec &d, RegCtrl &ctrl) const
	{
		ctrl = RegCtrl();
		d = SigSpec();
		for (auto bit : sigmap(s)) {
			if (!bit.wire) {
				d.append(bit);
				continue;
			}
			auto it = q2d.find(bit);
			if (it == q2d.end())
				return false;
			if (ctrl.clk == SigBit())
				ctrl = it->second.second;
			else if (ctrl != it->second.second)
				return false;
			d.append(it->second.first);
		}
		return true;
	}
};

static SigBit ctrl_bit(Module *module, SigBit b, bool inv)
{
	return inv ? module->NotGate(NEW_ID, b) : b;
}

static void absorb_lane_regs(Module *module)
{
	SigMap sigmap(module);
	FlopIndex flops(module, sigmap);
	auto through_regs = [&](SigSpec s, SigSpec &d, RegCtrl &ctrl) { return flops.through(s, d, ctrl); };


	// ctrl_bit may add cells, so walk a copy.
	std::vector<Cell*> cells(module->cells().begin(), module->cells().end());
	for (auto cell : cells) {
		if (cell->type != ID($__NX_MAC9X9WIDE_4LANE))
			continue;

		SigBit clk = cell->hasPort(ID::CLK) ? sigmap(cell->getPort(ID::CLK)[0]) : SigBit();
		if (clk == State::S0)
			clk = SigBit();
		// Control set chosen so far for the A and B side of each lane pair.
		RegCtrl a_ctrl[2], b_ctrl[2];
		bool a_used[2] = {false, false}, b_used[2] = {false, false};

		for (int lane = 0; lane < 4; lane++) {
			IdString pa = stringf("\\A%d", lane), pb = stringf("\\B%d", lane);
			SigSpec da, db;
			RegCtrl ca, cb;
			if (!through_regs(cell->getPort(pa), da, ca) || !through_regs(cell->getPort(pb), db, cb))
				continue;
			bool ra = ca.clk != SigBit(), rb = cb.clk != SigBit();
			if (!ra && !rb)
				continue;
			// One clock for the lane, the cell and any accumulator register.
			SigBit lane_clk = ra ? ca.clk : cb.clk;
			if ((ra && ca.clk != lane_clk) || (rb && cb.clk != lane_clk))
				continue;
			if (clk != SigBit() && lane_clk != clk)
				continue;
			// The lane pair shares each side's enable and reset.
			int pair = lane / 2;
			if (ra && a_used[pair] && ca != a_ctrl[pair])
				continue;
			if (rb && b_used[pair] && cb != b_ctrl[pair])
				continue;

			clk = lane_clk;
			if (ra) {
				a_ctrl[pair] = ca;
				a_used[pair] = true;
			}
			if (rb) {
				b_ctrl[pair] = cb;
				b_used[pair] = true;
			}
			cell->setPort(pa, da);
			cell->setPort(pb, db);
			cell->setParam(stringf("\\REGINPUTAB%d", lane), std::string("REGISTER"));
		}

		if (!a_used[0] && !a_used[1] && !b_used[0] && !b_used[1])
			continue;
		cell->setPort(ID::CLK, clk);
		const char *pair_name[2] = {"0A1", "2A3"};
		const char *pair_name_b[2] = {"0B1", "2B3"};
		for (int pair = 0; pair < 2; pair++) {
			if (a_used[pair]) {
				cell->setPort(stringf("\\CEA%s", pair_name[pair]), ctrl_bit(module, a_ctrl[pair].en, a_ctrl[pair].en_inv));
				cell->setPort(stringf("\\RSTA%s", pair_name[pair]), ctrl_bit(module, a_ctrl[pair].rst, a_ctrl[pair].rst_inv));
			}
			if (b_used[pair]) {
				cell->setPort(stringf("\\CEB%s", pair_name_b[pair]), ctrl_bit(module, b_ctrl[pair].en, b_ctrl[pair].en_inv));
				cell->setPort(stringf("\\RSTB%s", pair_name_b[pair]), ctrl_bit(module, b_ctrl[pair].rst, b_ctrl[pair].rst_inv));
			}
		}
	}
}

// A multiply wider than one ECP5 MULT18X18D arrives from mul2dsp as
// $__MUL18X18 tiles summed by an $add tree. A register on the product is
// moved back across that tree onto each tile's output register (other
// leaves get a fabric flop), and registers on the operands are copied into
// each tile's input registers. Every tile then runs fully registered, and
// the adder tree becomes a pipeline stage of its own.
//
// The tree is linear, so moving the register is sound when nothing else
// reads the tree, every leaf register shares the product register's clock,
// enable and reset, and the tree maps all-zero leaves to zero (so a reset or
// zero power-up value still gives zero).
static void retime_split_muls(Module *module)
{
	SigMap sigmap(module);
	dict<SigBit, Cell*> driver;
	dict<SigBit, pool<Cell*>> readers;
	pool<SigBit> ext_bits;
	for (auto cell : module->cells())
		for (auto &conn : cell->connections()) {
			// $__MUL18X18 tiles are not built-in cells: their only output is Y.
			bool is_input = cell->known() ? cell->input(conn.first) : conn.first != ID::Y;
			for (auto bit : sigmap(conn.second)) {
				if (!bit.wire)
					continue;
				if (is_input)
					readers[bit].insert(cell);
				else
					driver[bit] = cell;
			}
		}
	for (auto wire : module->wires())
		if (wire->port_output || wire->get_bool_attribute(ID::keep))
			for (auto bit : sigmap(SigSpec(wire)))
				ext_bits.insert(bit);

	// Is bit read only by cells in set (and not by an output or kept wire)?
	auto read_only_by = [&](SigBit bit, const pool<Cell*> &set) {
		if (ext_bits.count(bit))
			return false;
		auto it = readers.find(bit);
		if (it != readers.end())
			for (auto c : it->second)
				if (!set.count(c))
					return false;
		return true;
	};

	std::vector<Cell*> ffs;
	for (auto ff : module->cells())
		if (dsp_like_ff(ff))
			ffs.push_back(ff);

	pool<Cell*> removed;
	for (auto ff : ffs) {
		if (removed.count(ff))
			continue;
		SigSpec d = sigmap(ff->getPort(ID::D));
		SigSpec q = ff->getPort(ID::Q);
		bool q_zero_init = true;
		for (auto b : q)
			q_zero_init = q_zero_init && zero_or_no_init(b);
		if (!q_zero_init || !d[0].wire || !driver.count(d[0]))
			continue;
		Cell *root = driver.at(d[0]);
		if (root->type != ID($add))
			continue;
		SigSpec root_y = sigmap(root->getPort(ID::Y));
		if (GetSize(root_y) < GetSize(d) || root_y.extract(0, GetSize(d)) != d)
			continue;
		bool has_reset_or_zero = ff->type.in(ID($sdff), ID($sdffe));
		for (auto b : q)
			if (b.wire && b.wire->attributes.count(ID::init))
				has_reset_or_zero = true;

		// Collect the tree: $add cells whose outputs feed only the tree.
		pool<Cell*> tree = {root};
		std::vector<Cell*> work = {root};
		pool<Cell*> tiles;
		pool<SigBit> fabric_leaves;
		bool ok = true;
		while (ok && !work.empty()) {
			Cell *add = work.back();
			work.pop_back();
			for (auto p : {ID::A, ID::B})
				for (auto bit : sigmap(add->getPort(p))) {
					if (!bit.wire) {
						// A constant only survives a reset if it is zero.
						if (has_reset_or_zero && bit != State::S0)
							ok = false;
						continue;
					}
					Cell *drv = driver.count(bit) ? driver.at(bit) : nullptr;
					if (drv && drv->type == ID($add) && !tree.count(drv)) {
						tree.insert(drv);
						work.push_back(drv);
					} else if (drv && drv->type == ID($__MUL18X18)) {
						tiles.insert(drv);
					} else if (!drv || drv->type != ID($add)) {
						fabric_leaves.insert(bit);
					}
				}
		}
		if (!ok || tiles.empty())
			continue;
		// Only the product register may read the root; everything else in
		// the tree, and every tile, only feeds the tree.
		pool<Cell*> tree_and_ff = tree;
		tree_and_ff.insert(ff);
		for (auto b : root_y)
			if (b.wire && !read_only_by(b, tree_and_ff))
				ok = false;
		for (auto add : tree)
			if (add != root)
				for (auto b : sigmap(add->getPort(ID::Y)))
					if (b.wire && !read_only_by(b, tree))
						ok = false;
		for (auto tile : tiles) {
			if (tile->hasParam(ID(REGOUTPUT)) && tile->getParam(ID(REGOUTPUT)).decode_string() != "BYPASS")
				ok = false;
			for (auto b : sigmap(tile->getPort(ID::Y)))
				if (b.wire && !read_only_by(b, tree))
					ok = false;
		}
		for (auto b : fabric_leaves)
			if (!read_only_by(b, tree))
				ok = false;
		if (!ok)
			continue;

		RegCtrl ctrl = ctrl_of(sigmap, ff);
		SigBit ce = ctrl_bit(module, ctrl.en, ctrl.en_inv);
		SigBit rst = ctrl_bit(module, ctrl.rst, ctrl.rst_inv);

		// Operand registers, looked up before the product register goes.
		FlopIndex flops(module, sigmap);
		for (auto tile : tiles) {
			tile->setPort(ID::CLK, ctrl.clk);
			tile->setPort(ID(CEOUT), ce);
			tile->setPort(ID(RSTOUT), rst);
			tile->setParam(ID(REGOUTPUT), std::string("REGISTER"));
			tile->setParam(ID(RESETMODE), std::string("SYNC"));

			// Both inputs or neither, for nextpnr-ecp5's timing model.
			SigSpec da, db;
			RegCtrl ca, cb;
			if (!flops.through(tile->getPort(ID::A), da, ca) || !flops.through(tile->getPort(ID::B), db, cb))
				continue;
			if (ca.clk != ctrl.clk || cb.clk != ctrl.clk)
				continue;
			tile->setPort(ID::A, da);
			tile->setPort(ID::B, db);
			tile->setPort(ID(CEA), ctrl_bit(module, ca.en, ca.en_inv));
			tile->setPort(ID(RSTA), ctrl_bit(module, ca.rst, ca.rst_inv));
			tile->setPort(ID(CEB), ctrl_bit(module, cb.en, cb.en_inv));
			tile->setPort(ID(RSTB), ctrl_bit(module, cb.rst, cb.rst_inv));
			tile->setParam(ID(REGINPUTA), std::string("REGISTER"));
			tile->setParam(ID(REGINPUTB), std::string("REGISTER"));
		}

		// A leaf that is not a tile gets a fabric copy of the register.
		if (!fabric_leaves.empty()) {
			SigSpec leaf_d, leaf_q;
			for (auto b : fabric_leaves) {
				leaf_d.append(b);
				leaf_q.append(module->addWire(NEW_ID));
			}
			Cell *lf = module->addCell(NEW_ID, ff->type);
			lf->parameters = ff->parameters;
			lf->setParam(ID::WIDTH, GetSize(leaf_d));
			if (lf->hasParam(ID::SRST_VALUE))
				lf->setParam(ID::SRST_VALUE, Const(State::S0, GetSize(leaf_d)));
			for (auto &conn : ff->connections())
				if (!conn.first.in(ID::D, ID::Q))
					lf->setPort(conn.first, conn.second);
			lf->setPort(ID::D, leaf_d);
			lf->setPort(ID::Q, leaf_q);
			dict<SigBit, SigBit> remap;
			for (int i = 0; i < GetSize(leaf_d); i++)
				remap[leaf_d[i]] = leaf_q[i];
			for (auto add : tree)
				for (auto p : {ID::A, ID::B}) {
					SigSpec s = sigmap(add->getPort(p));
					for (auto &b : s)
						if (remap.count(b))
							b = remap.at(b);
					add->setPort(p, s);
				}
		}

		log("  Moving register %s back across %d adders onto %d DSP tiles and %d fabric bits.\n",
				log_id(ff), GetSize(tree), GetSize(tiles), GetSize(fabric_leaves));
		module->connect(q, ff->getPort(ID::D));
		removed.insert(ff);
		module->remove(ff);
	}
}

// A register on an ECP5 block RAM's read data moves into the DP16KD port's
// own output register (REGMODE=OUTREG, enabled by OCE). The RAM's
// clock-to-out without it is slow, and nextpnr times the registered mode.
//
// Only a port in a normal width takes it: in the 36-bit mode the read data
// spans both DOA and DOB with OCEA tied off. The port's reset must be tied
// low, and the register has none, since how RST acts on the output register
// is left out of it; its enable becomes OCE.
static void absorb_bram_outregs(Module *module)
{
	SigMap sigmap(module);
	dict<SigBit, pool<Cell*>> readers;
	pool<SigBit> ext_bits;
	for (auto cell : module->cells())
		for (auto &conn : cell->connections()) {
			if (cell->known() && !cell->input(conn.first))
				continue;
			// A DP16KD's own pins are not readers of its data outputs.
			if (cell->type == ID(DP16KD) && conn.first.begins_with("\\DO"))
				continue;
			for (auto bit : sigmap(conn.second))
				if (bit.wire)
					readers[bit].insert(cell);
		}
	for (auto wire : module->wires())
		if (wire->port_output || wire->get_bool_attribute(ID::keep))
			for (auto bit : sigmap(SigSpec(wire)))
				ext_bits.insert(bit);

	std::vector<Cell*> rams;
	for (auto cell : module->cells())
		if (cell->type == ID(DP16KD))
			rams.push_back(cell);
	// Emptied flops go at the end: removing cells mid-walk upsets the module.
	pool<Cell*> emptied;
	for (auto ram : rams) {
		auto int_param = [&](IdString p) { return ram->hasParam(p) ? ram->getParam(p).as_int() : 18; };
		if (int_param(ID(DATA_WIDTH_A)) == 36 || int_param(ID(DATA_WIDTH_B)) == 36)
			continue;

		for (char port : {'A', 'B'}) {
			auto pin = [&](const char *name) { return IdString(stringf("\\%s%c", name, port)); };
			IdString regmode = stringf("\\REGMODE_%c", port);
			if (ram->hasParam(regmode) && ram->getParam(regmode).decode_string() != "NOREG")
				continue;
			if (!ram->hasPort(pin("RST")) || sigmap(ram->getPort(pin("RST"))) != SigSpec(State::S0))
				continue;
			IdString clkmux = stringf("\\CLK%cMUX", port);
			if (ram->hasParam(clkmux) && ram->getParam(clkmux).decode_string() != stringf("CLK%c", port))
				continue;
			if (!ram->hasPort(pin("CLK")))
				continue;
			SigBit clk = sigmap(ram->getPort(pin("CLK")))[0];

			// Each data bit that anything reads must feed exactly one flop.
			dict<int, std::pair<Cell*, int>> taken;
			RegCtrl ctrl;
			bool first = true, ok = true;
			for (int i = 0; i < 18 && ok; i++) {
				IdString dop = stringf("\\DO%c%d", port, i);
				if (!ram->hasPort(dop))
					continue;
				SigBit bit = sigmap(ram->getPort(dop))[0];
				if (!bit.wire)
					continue;
				auto it = readers.find(bit);
				if (!ext_bits.count(bit) && (it == readers.end() || it->second.empty()))
					continue;
				if (ext_bits.count(bit) || GetSize(it->second) != 1) {
					ok = false;
					break;
				}
				Cell *ff = *it->second.begin();
				if (!ff->type.in(ID($dff), ID($dffe)) || !dsp_like_ff(ff)) {
					ok = false;
					break;
				}
				// The bit may appear only once, and only on D.
				SigSpec d = sigmap(ff->getPort(ID::D));
				int idx = -1, hits = 0;
				for (int j = 0; j < GetSize(d); j++)
					if (d[j] == bit)
						idx = j, hits++;
				if (hits != 1 || sigmap(ff->getPort(ID::CLK))[0] == bit ||
						(ff->hasPort(ID::EN) && sigmap(ff->getPort(ID::EN))[0] == bit) ||
						!zero_or_no_init(ff->getPort(ID::Q)[idx])) {
					ok = false;
					break;
				}
				RegCtrl c = ctrl_of(sigmap, ff);
				if (c.clk != clk || (!first && c != ctrl))
					ok = false;
				ctrl = c;
				first = false;
				taken[i] = {ff, idx};
			}
			if (!ok || taken.empty())
				continue;

			// The output register drives what the flops drove.
			dict<Cell*, pool<int>> cut;
			for (auto &t : taken) {
				ram->setPort(stringf("\\DO%c%d", port, t.first), t.second.first->getPort(ID::Q)[t.second.second]);
				cut[t.second.first].insert(t.second.second);
			}
			for (auto &c : cut) {
				Cell *ff = c.first;
				SigSpec d = ff->getPort(ID::D), q = ff->getPort(ID::Q), nd, nq;
				for (int j = 0; j < GetSize(d); j++)
					if (!c.second.count(j)) {
						nd.append(d[j]);
						nq.append(q[j]);
					}
				if (GetSize(nd) == 0) {
					emptied.insert(ff);
					continue;
				}
				ff->setPort(ID::D, nd);
				ff->setPort(ID::Q, nq);
				ff->setParam(ID::WIDTH, GetSize(nd));
			}
			ram->setParam(regmode, std::string("OUTREG"));
			ram->setPort(pin("OCE"), ctrl_bit(module, ctrl.en, ctrl.en_inv));
			log("  Moving %d read-data flops into the output register of %s port %c.\n",
					GetSize(taken), log_id(ram), port);
		}
	}
	for (auto ff : emptied)
		module->remove(ff);
}

// Mark ECP5 flops that sit right at a pin for nextpnr to place in that pin's
// IOLOGIC (syn_useioff): an input flop whose DI is the pin and the pin's only
// reader, and an output flop whose Q drives the pin and nothing else. Those
// are the conditions nextpnr's packer checks. It also requires the pin to be
// LOC-constrained, which is why this is opt-in.
static void mark_io_ffs(Module *module)
{
	SigMap sigmap(module);
	dict<SigBit, pool<std::pair<Cell*, IdString>>> readers;
	for (auto cell : module->cells())
		for (auto &conn : cell->connections()) {
			if (cell->known() && !cell->input(conn.first))
				continue;
			for (auto bit : sigmap(conn.second))
				if (bit.wire)
					readers[bit].insert({cell, conn.first});
		}
	pool<SigBit> in_pins, out_pins, keep_bits;
	for (auto wire : module->wires()) {
		for (auto bit : sigmap(SigSpec(wire))) {
			if (wire->port_input && !wire->port_output)
				in_pins.insert(bit);
			if (wire->port_output && !wire->port_input)
				out_pins.insert(bit);
			if (wire->get_bool_attribute(ID::keep))
				keep_bits.insert(bit);
		}
	}
	// A bit the output port and a kept wire both see is still one reader.
	auto only_reader = [&](SigBit bit, Cell *cell, IdString port) {
		auto it = readers.find(bit);
		return it != readers.end() && GetSize(it->second) == 1 && *it->second.begin() == std::make_pair(cell, port);
	};
	int n_in = 0, n_out = 0;
	for (auto ff : module->cells()) {
		if (ff->type != ID(TRELLIS_FF) || ff->has_attribute(ID(syn_useioff)))
			continue;
		SigBit di = ff->hasPort(ID(DI)) ? sigmap(ff->getPort(ID(DI)))[0] : SigBit();
		SigBit q = ff->hasPort(ID::Q) ? sigmap(ff->getPort(ID::Q))[0] : SigBit();
		if (di.wire && in_pins.count(di) && !keep_bits.count(di) && only_reader(di, ff, ID(DI))) {
			ff->set_bool_attribute(ID(syn_useioff));
			ff->set_string_attribute(ID(ioff_dir), "input");
			n_in++;
		} else if (q.wire && out_pins.count(q) && !keep_bits.count(q) && !readers.count(q)) {
			ff->set_bool_attribute(ID(syn_useioff));
			ff->set_string_attribute(ID(ioff_dir), "output");
			n_out++;
		}
	}
	if (n_in || n_out)
		log("  Marked %d input and %d output flops for the IO registers in %s.\n", n_in, n_out, log_id(module));
}

#include "techlibs/lattice/lattice_dsp_nexus_pm.h"

struct LatticeDspNexusPass : public Pass {
	LatticeDspNexusPass() : Pass("lattice_dsp_nexus", "Lattice Nexus DSP inference") { }
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    lattice_dsp_nexus [options] [selection]\n");
		log("\n");
		log("Infer Lattice Nexus sysDSP macrocells (MULTADDSUB18X18,\n");
		log("MULTADDSUB36X36, MULTPREADD18X18, MULTADDSUB9X9WIDE) from MAC\n");
		log("and dot-product patterns, including a 36x36 multiply-add and a\n");
		log("4-lane 9x9 dot product with an accumulate, and absorb the\n");
		log("pipeline flip-flops around bare MULT18X18 / MULT36X36 multipliers\n");
		log("into the hardened DSP input and output registers. A MAC whose sum\n");
		log("is registered and fed back as its own addend (p <= p +/- A*B)\n");
		log("becomes an accumulator in the DSP output register. The sum bits a\n");
		log("MAC cannot reach are tied off, so a multiply by that sum (a scale\n");
		log("after a dot product) is matched at its true width.\n");
		log("\n");
		log("    -wide-scale\n");
		log("        when a scale's sign and exponent move into the narrow operand of a\n");
		log("        multiply, let that operand grow past 18 bits, so the multiply takes\n");
		log("        MULT18X36 instead of MULT18X18. For an fp16 scale this moves 4 of\n");
		log("        the 5 exponent shift levels into the DSP instead of 2, for a second\n");
		log("        multiplier core. It saves LUTs but not always time: MULT18X36 is\n");
		log("        slower and the operand shifter in front of it deeper, so check Fmax.\n");
		log("\n");
		log("    -dsp-shift\n");
		log("        map a variable left shift of a value up to 18 bits by 2 to 4 bits\n");
		log("        onto a multiply by the one-hot 1 << S, instead of a LUT barrel\n");
		log("        shifter. It takes a DSP block for each shift and saves LUTs, but the\n");
		log("        multiplier array is slower than a few LUT levels: expect lower Fmax.\n");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing LATTICE_DSP_NEXUS pass.\n");
		bool wide_scale = false, dsp_shift = false;
		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			if (args[argidx] == "-wide-scale") {
				wide_scale = true;
				continue;
			}
			if (args[argidx] == "-dsp-shift") {
				dsp_shift = true;
				continue;
			}
			break;
		}
		extra_args(args, argidx, design);

		for (auto module : design->selected_modules()) {
			// Before the MAC patterns, so a sign-selected accumulate still fuses.
			fold_scale_into_mul(module, false, wide_scale);
			{
				lattice_dsp_nexus_pm pm(module, module->cells());

				// Consume dot4+C before the bare dot4, and the wide MAC after the
				// 18-bit MAC, so the narrower mapping still wins.
				pm.run_nexus_mac9_4lane_c();
				pm.run_nexus_mac9_4lane_c_pre();
				pm.run_nexus_mac9_4lane();
				// A longer dot is a 4-lane block plus a prefix. Peel those
				// groups before nexus_mac18 widens each leftover product.
				for (int i = 0; i < 8; i++)
					if (!pm.run_nexus_mac9_4lane_tail())
						break;
				pm.run_nexus_mac18();
				pm.run_nexus_mac36();
				pm.run_nexus_preadd18();
			}

			bound_mac_outputs(module);
			bound_arith(module);
			// After the bound, so a shifted dot shows its true width.
			if (dsp_shift)
				shift_to_mul(module);
			narrow_mul_operands(module);

			// Register absorption needs the narrowed widths, and the MAC cells
			// made above are not in the first matcher's index.
			// The accumulator goes first: it moves its register into the DSP,
			// and a multiply that also reads that register must not have
			// copied it already.
			{
				lattice_dsp_nexus_pm pm(module, module->cells());
				pm.run_nexus_mac_acc();
				pm.run_nexus_mul_reg();
			}

			// After the matchers are gone, so their removed flops are gone too.
			absorb_lane_regs(module);
		}
	}
} LatticeDspNexusPass;

// Sums of products that fit 18x18 go to ALU54B, each fed by a pair of
// MULT18X18D and holding the register in its output register
// ($__ECP5_MAC54):
//
//   acc <= acc +- P0 +- P1 ...     a running sum, also as a reload
//                                  (clr ? P.. : acc + P..) or with the acc
//                                  term gated to zero
//   y <= +- P0 +- P1 ... [+- C]    a registered sum; C is one other term,
//                                  a constant (the ALU's RNDPAT) or a signal
//                                  (its C input)
//
// An ALU adds MA and adds or subtracts MB, so the products go in pairs, a
// negative one in the B slot. With more pairs than one, each ALU keeps a
// partial sum and the fabric adds them after the registers: the partials
// start, reload and reset together, so their sum is the register the RTL
// has. The ALU sums exactly, and the low bits of a sum do not depend on the
// bits above them, so a register up to 52 bits wraps just as in the RTL. The
// ALU result is 54 bits wide, but the data sheet (FPGA-DS-02012) promises an
// accumulator of 52 bits, so no more is used.
static void infer_ecp5_macs(Module *module, bool mac1, const pool<IdString> &exact_sums)
{
	SigMap sigmap(module);
	dict<SigBit, Cell*> driver;
	dict<SigBit, pool<Cell*>> readers;
	pool<SigBit> outputs;
	for (auto cell : module->cells())
		for (auto &conn : cell->connections()) {
			// An unknown cell's ports count as reads, never as drivers.
			bool out = cell->known() && cell->output(conn.first);
			for (auto bit : sigmap(conn.second)) {
				if (!bit.wire)
					continue;
				if (out)
					driver[bit] = cell;
				else
					readers[bit].insert(cell);
			}
		}
	for (auto wire : module->wires())
		if (wire->port_output)
			for (auto bit : sigmap(wire))
				outputs.insert(bit);
	auto driver_of = [&](const SigSpec &s) -> Cell* {
		if (GetSize(s) == 0 || !driver.count(s[0]))
			return nullptr;
		return driver.at(s[0]);
	};

	// Flop Q bits, for the operand registers of the MULT18X18D.
	struct QInfo { SigBit d; RegCtrl ctrl; Cell *ff; };
	dict<SigBit, QInfo> q_info;
	for (auto cell : module->cells())
		if (dsp_like_ff(cell)) {
			RegCtrl c = ctrl_of(sigmap, cell);
			SigSpec cq = cell->getPort(ID::Q), cd = cell->getPort(ID::D);
			for (int i = 0; i < GetSize(cq); i++)
				if (zero_or_no_init(cq[i]))
					q_info[sigmap(cq[i])] = {cd[i], c, cell};
		}

	pool<Cell*> used, copied;

	// s through its flops: every bit a Q with one control set (constant zero
	// bits pass), from no flop already absorbed or about to be (self).
	auto operand_flops = [&](const SigSpec &s, SigSpec &d, RegCtrl &c, Cell *self, pool<Cell*> &srcs) {
		c = RegCtrl();
		d = SigSpec();
		for (auto bit : sigmap(s)) {
			if (!bit.wire) {
				if (bit != State::S0)
					return false;
				d.append(bit);
				continue;
			}
			auto it = q_info.find(bit);
			if (it == q_info.end() || used.count(it->second.ff) || it->second.ff == self)
				return false;
			if (c.clk == SigBit())
				c = it->second.ctrl;
			else if (c != it->second.ctrl)
				return false;
			d.append(it->second.d);
			srcs.insert(it->second.ff);
		}
		return c.clk != SigBit();
	};

	struct Terms {
		std::vector<std::pair<Cell*, bool>> prods;	// product, negated
		pool<Cell*> cells;
		int nacc = 0;
		Cell *gate = nullptr;
		bool gate_inv = false;
		dict<Cell*, Cell*> pipe;	// product -> its register, for REG_PIPELINE
		int naddend = 0;
		SigSpec addend;		// 54 bits
		bool addend_neg = false;
	};

	std::vector<Cell*> ffs;
	for (auto cell : module->cells())
		if (dsp_like_ff(cell))
			ffs.push_back(cell);

	for (auto ff : ffs) {
		SigSpec q = sigmap(ff->getPort(ID::Q));
		SigSpec d = sigmap(ff->getPort(ID::D));
		int w = GetSize(q);
		if (w < 2 || w > 52 || copied.count(ff) || used.count(ff))
			continue;
		bool init_ok = true;
		for (auto bit : ff->getPort(ID::Q))
			if (!bit.wire || !zero_or_no_init(bit))
				init_ok = false;
		if (!init_ok)
			continue;
		RegCtrl ctrl = ctrl_of(sigmap, ff);

		// A product through one flop on the ALU's clock: the flop becomes the
		// MULT18X18D pipeline register. Returns the product, and in sd the
		// flop's view of s from before it.
		auto registered_product = [&](Cell *pff, const SigSpec &s, SigSpec &sd) -> Cell* {
			if (!dsp_like_ff(pff) || pff == ff || copied.count(pff) || ctrl_of(sigmap, pff).clk != ctrl.clk)
				return nullptr;
			for (auto bit : pff->getPort(ID::Q))
				if (!bit.wire || !zero_or_no_init(bit))
					return nullptr;
			SigSpec pq = sigmap(pff->getPort(ID::Q)), pd = sigmap(pff->getPort(ID::D));
			dict<SigBit, SigBit> q2d;
			for (int i = 0; i < GetSize(pq); i++)
				q2d[pq[i]] = pd[i];
			sd = SigSpec();
			for (auto bit : s) {
				if (!bit.wire)
					sd.append(bit);
				else if (q2d.count(bit))
					sd.append(q2d.at(bit));
				else
					return nullptr;
			}
			Cell *mul = driver_of(sd);
			if (!mul || mul->type != ID($mul) || used.count(mul))
				return nullptr;
			return mul;
		};

		// Collect the terms of s, read with signedness sgn and extended to
		// width sw, into t. Each term must reach the sum exactly: a product
		// whole, a partial sum that cannot overflow its own width, or one
		// at least sw bits wide (the bits above sw do not matter). Anything
		// else is an opaque addend, taken at the value the RTL gives it.
		std::function<bool(const SigSpec&, bool, int, Terms&, int, bool)> collect;
		collect = [&](const SigSpec &s, bool sgn, int sw, Terms &t, int depth, bool neg) -> bool {
			SigSpec seen = s;
			seen.extend_u0(sw, sgn);
			if (sw == w && seen == q) {
				if (neg)
					return false;
				t.nacc++;
				return true;
			}
			Cell *src = driver_of(s);
			// A cell reached twice (p + p) would be summed once.
			bool open = src && !used.count(src) && !t.cells.count(src) && depth <= 24;
			// A product, straight or through its register.
			Cell *mul = nullptr, *pff = nullptr;
			SigSpec ms = s;
			if (open && src->type == ID($mul))
				mul = src;
			else if (open && dsp_like_ff(src)) {
				SigSpec sd;
				Cell *m = registered_product(src, s, sd);
				if (m && !t.cells.count(m))
					mul = m, pff = src, ms = sd;
			}
			if (mul) {
				SigSpec y = sigmap(mul->getPort(ID::Y));
				bool msgn = mul->getParam(ID::A_SIGNED).as_bool();
				int aw = GetSize(mul->getPort(ID::A)), bw = GetSize(mul->getPort(ID::B));
				if (msgn == mul->getParam(ID::B_SIGNED).as_bool() && aw <= 18 && bw <= 18 &&
						exact_join(y, aw + bw, msgn, ms, sgn, sw)) {
					t.prods.push_back({mul, neg});
					t.cells.insert(mul);
					if (pff) {
						t.cells.insert(pff);
						t.pipe[mul] = pff;
					}
					return true;
				}
			}
			if (open && src->type.in(ID($add), ID($sub))) {
				SigSpec y = sigmap(src->getPort(ID::Y));
				bool asgn = add_is_signed(src);
				bool sub = src->type == ID($sub);
				int yw = GetSize(y);
				// Operand widths without their extension: bound_arith narrows Y
				// to the sum's true width but leaves the ports wide. An unsigned
				// difference can go below zero, so it is exact only mod 2^yw.
				int vw = std::max(value_width(sigmap, src->getPort(ID::A), asgn),
						value_width(sigmap, src->getPort(ID::B), asgn));
				bool exact = (yw > vw && (!sub || asgn)) || exact_sums.count(src->name);
				if ((exact || yw >= sw) && exact_join(y, exact ? yw : INT_MAX, asgn, s, sgn, sw)) {
					int ow = std::min(yw, sw);
					Terms saved = t;
					t.cells.insert(src);
					if (collect(sigmap(src->getPort(ID::A)), asgn, ow, t, depth + 1, neg) &&
							collect(sigmap(src->getPort(ID::B)), asgn, ow, t, depth + 1, neg != sub))
						return true;
					t = saved;
				}
			}
			// acc or zero: the sum restarts from the products alone.
			if (open && src->type == ID($mux) && sw == w && !neg && !t.gate &&
					s == sigmap(src->getPort(ID::Y)) && GetSize(s) == w) {
				SigSpec ma = sigmap(src->getPort(ID::A)), mb = sigmap(src->getPort(ID::B));
				bool inv = ma == q && mb.is_fully_zero();
				if (inv || (mb == q && ma.is_fully_zero())) {
					t.gate = src;
					t.gate_inv = inv;
					t.cells.insert(src);
					t.nacc++;
					return true;
				}
			}
			t.naddend++;
			t.addend = seen;
			t.addend.extend_u0(54, sgn);
			t.addend_neg = neg;
			return true;
		};

		Terms t;
		SigBit sel;
		bool sel_inv = false;
		Cell *root = driver_of(d);
		if (!root || used.count(root))
			continue;
		auto same_prods = [](std::vector<std::pair<Cell*, bool>> a, std::vector<std::pair<Cell*, bool>> b) {
			std::sort(a.begin(), a.end());
			std::sort(b.begin(), b.end());
			return a == b;
		};
		if (root->type == ID($mux) && sigmap(root->getPort(ID::Y)) == d) {
			// clr ? load : accumulate, with load the same products as
			// accumulate and no acc term.
			Terms ta, tb;
			if (!collect(sigmap(root->getPort(ID::A)), false, w, ta, 0, false) ||
					!collect(sigmap(root->getPort(ID::B)), false, w, tb, 0, false))
				continue;
			Terms &acc = ta.nacc ? ta : tb, &load = ta.nacc ? tb : ta;
			if (acc.nacc != 1 || acc.gate || load.nacc != 0 || acc.naddend || load.naddend ||
					!same_prods(acc.prods, load.prods) || acc.pipe != load.pipe)
				continue;
			t.prods = acc.prods;
			t.pipe = acc.pipe;
			t.cells = acc.cells;
			for (auto c : load.cells)
				t.cells.insert(c);
			t.cells.insert(root);
			t.nacc = 1;
			sel = sigmap(root->getPort(ID::S)[0]);
			// LOADN is high while accumulating. Y = S ? B : A, so with the
			// acc on A that is while S is low.
			sel_inv = ta.nacc != 0;
		} else {
			if (!collect(d, false, w, t, 0, false) || t.nacc > 1)
				continue;
			if (t.gate) {
				sel = sigmap(t.gate->getPort(ID::S)[0]);
				sel_inv = t.gate_inv;
			}
		}
		bool accumulate = t.nacc == 1;
		// The C input takes the acc or one addend.
		if (t.naddend > 1 || (accumulate && t.naddend))
			continue;
		// A registered single product is the MULT18X18D's own register.
		if (t.prods.empty() || (!accumulate && GetSize(t.prods) + t.naddend < 2) || GetSize(t.prods) > 16)
			continue;

		// Pair the products: a negative one only fits the B slot.
		struct Group { Cell *a = nullptr, *b = nullptr; bool bsub = false; };
		std::vector<Cell*> pos, negs;
		for (auto &p : t.prods)
			(p.second ? negs : pos).push_back(p.first);
		std::vector<Group> groups;
		int singles = 0;
		while (!pos.empty() && !negs.empty()) {
			groups.push_back({pos.back(), negs.back(), true});
			pos.pop_back(), negs.pop_back();
		}
		while (GetSize(pos) >= 2) {
			groups.push_back({pos[GetSize(pos) - 1], pos[GetSize(pos) - 2], false});
			pos.pop_back(), pos.pop_back();
		}
		for (auto p : pos)
			groups.push_back({p, nullptr, false}), singles++;
		for (auto p : negs)
			groups.push_back({nullptr, p, true}), singles++;
		// Both MULT18X18D of a pair are taken either way, so a lone product
		// costs a multiplier: only with -mac1.
		if (singles && !mac1)
			continue;

		// Everything absorbed must be read only by other absorbed cells.
		pool<Cell*> absorbed = t.cells;
		absorbed.insert(ff);
		bool private_ok = true;
		for (auto c : t.cells)
			for (auto bit : sigmap(c->getPort(c->hasPort(ID::Y) ? ID::Y : ID::Q))) {
				if (!bit.wire)
					continue;
				if (outputs.count(bit))
					private_ok = false;
				if (readers.count(bit))
					for (auto r : readers.at(bit))
						if (!absorbed.count(r))
							private_ok = false;
			}
		if (!private_ok)
			continue;

		log("  %s: %s <= %s%d product(s)%s -> %d ALU54B\n", log_id(module), log_signal(q),
				!accumulate ? "" : sel == SigBit() ? "acc + " : "(acc or 0) + ", GetSize(t.prods),
				t.naddend ? " + addend" : "", GetSize(groups));
		SigBit ce = ctrl_bit(module, ctrl.en, ctrl.en_inv);
		SigBit rst = ctrl_bit(module, ctrl.rst, ctrl.rst_inv);
		SigBit loadn = sel == SigBit() ? SigBit(State::S1) : ctrl_bit(module, sel, sel_inv);
		SigSpec total;
		for (int g = 0; g < GetSize(groups); g++) {
			Cell *mac = module->addCell(NEW_ID, ID($__ECP5_MAC54));
			mac->setParam(ID::Y_WIDTH, w);
			// Enables high and resets low unless a register below uses them.
			for (auto p : {ID(CEA0), ID(CEB0), ID(CEP0), ID(CEA1), ID(CEB1), ID(CEP1)})
				mac->setPort(p, State::S1);
			for (auto p : {ID(RSTA0), ID(RSTB0), ID(RSTP0), ID(RSTA1), ID(RSTB1), ID(RSTP1)})
				mac->setPort(p, State::S0);
			Cell *slot[2] = {groups[g].a, groups[g].b};
			for (int k = 0; k < 2; k++) {
				SigSpec a(State::S0, 18), b(State::S0, 18);
				bool sgn = false;
				if (slot[k]) {
					sgn = slot[k]->getParam(ID::A_SIGNED).as_bool();
					a = slot[k]->getPort(ID::A);
					b = slot[k]->getPort(ID::B);
					a.extend_u0(18, sgn);
					b.extend_u0(18, sgn);
				}
				// Operands that come straight from flops on the ALU's clock go in
				// the MULT18X18D input registers, both or neither (nextpnr times
				// a MULT18X18D with one of them as if it had none). The DSP
				// registers start and reset at zero, so a constant one bit
				// cannot pass through them. The fabric flop stays for any other
				// reader; opt_clean drops it otherwise.
				RegCtrl ca, cb;
				SigSpec ad, bd;
				pool<Cell*> srcs;
				bool reg = slot[k] && operand_flops(a, ad, ca, ff, srcs) && operand_flops(b, bd, cb, ff, srcs) &&
						ca.clk == ctrl.clk && cb.clk == ctrl.clk;
				if (reg) {
					// Their D now feeds the DSP too: never absorb them as a sum.
					for (auto c : srcs)
						copied.insert(c);
					a = ad;
					b = bd;
					mac->setPort(k ? ID(CEA1) : ID(CEA0), ctrl_bit(module, ca.en, ca.en_inv));
					mac->setPort(k ? ID(RSTA1) : ID(RSTA0), ctrl_bit(module, ca.rst, ca.rst_inv));
					mac->setPort(k ? ID(CEB1) : ID(CEB0), ctrl_bit(module, cb.en, cb.en_inv));
					mac->setPort(k ? ID(RSTB1) : ID(RSTB0), ctrl_bit(module, cb.rst, cb.rst_inv));
				}
				mac->setParam(k ? ID(REGIN1) : ID(REGIN0), reg ? State::S1 : State::S0);
				// A registered product: its flop is the pipeline register.
				Cell *pff = slot[k] && t.pipe.count(slot[k]) ? t.pipe.at(slot[k]) : nullptr;
				if (pff) {
					RegCtrl cp = ctrl_of(sigmap, pff);
					mac->setPort(k ? ID(CEP1) : ID(CEP0), ctrl_bit(module, cp.en, cp.en_inv));
					mac->setPort(k ? ID(RSTP1) : ID(RSTP0), ctrl_bit(module, cp.rst, cp.rst_inv));
				}
				mac->setParam(k ? ID(PIPE1) : ID(PIPE0), pff ? State::S1 : State::S0);
				mac->setParam(k ? ID(SIGNED1) : ID(SIGNED0), sgn ? State::S1 : State::S0);
				mac->setPort(k ? ID(A1) : ID(A0), a);
				mac->setPort(k ? ID(B1) : ID(B0), b);
			}
			mac->setParam(ID(ASEL), slot[0] ? State::S1 : State::S0);
			mac->setParam(ID(BSEL), slot[1] ? State::S1 : State::S0);
			mac->setParam(ID(BSUB), groups[g].bsub ? State::S1 : State::S0);
			// CSEL: 0 zero, 1 the ALU's own result (acc), 2 the C input, 3 RNDPAT
			int csel = 0;
			bool csub = false;
			Const rnd(State::S0, 54);
			if (accumulate)
				csel = 1;
			else if (g == 0 && t.naddend) {
				if (t.addend.is_fully_const()) {
					// Fold the sign into the constant.
					rnd = t.addend.as_const();
					if (t.addend_neg)
						rnd = const_neg(rnd, Const(), true, false, 54);
					csel = 3;
				} else {
					csel = 2;
					csub = t.addend_neg;
					mac->setPort(ID::C, t.addend);
				}
			}
			mac->setParam(ID(CSEL), csel);
			mac->setParam(ID(CSUB), csub ? State::S1 : State::S0);
			mac->setParam(ID(RNDVAL), rnd);
			std::string hex = "0x";
			for (int i = 13; i >= 0; i--) {
				int nib = 0;
				for (int j = 0; j < 4; j++)
					if (4 * i + j < 54 && rnd[4 * i + j] == State::S1)
						nib |= 1 << j;
				hex += "0123456789ABCDEF"[nib];
			}
			mac->setParam(ID(RNDPAT), hex);
			mac->setPort(ID::CLK, ctrl.clk);
			mac->setPort(ID(CE), ce);
			mac->setPort(ID(RST), rst);
			mac->setPort(ID(LOADN), loadn);
			if (GetSize(groups) == 1) {
				mac->setPort(ID::Y, ff->getPort(ID::Q));
				continue;
			}
			SigSpec part = module->addWire(NEW_ID, w);
			mac->setPort(ID::Y, part);
			total = GetSize(total) ? module->Add(NEW_ID, total, part) : part;
		}
		if (GetSize(groups) > 1)
			module->connect(ff->getPort(ID::Q), total);

		for (auto c : absorbed)
			used.insert(c);
	}
	for (auto c : used)
		module->remove(c);
}

struct LatticeDspEcp5Pass : public Pass {
	LatticeDspEcp5Pass() : Pass("lattice_dsp_ecp5", "Lattice ECP5 DSP register inference") { }
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    lattice_dsp_ecp5 [options] [selection]\n");
		log("\n");
		log("Absorb the flip-flops around a multiply that fits one 18x18 MULT18X18D into\n");
		log("its input and output registers (REG_INPUTA, REG_INPUTB, REG_OUTPUT). An\n");
		log("input register is copied, so other readers keep the fabric flop; an output\n");
		log("register is moved only when nothing else reads the product.\n");
		log("\n");
		log("A register of up to 52 bits holding a sum of such products goes to ALU54B,\n");
		log("each fed by a pair of MULT18X18D and holding the register in its output\n");
		log("register: a running sum acc <= acc +- P0 +- P1 ... (also as a reload,\n");
		log("acc <= clr ? P.. : acc + P..), or a registered y <= +- P0 +- P1 ... with\n");
		log("at most one other term, a constant (RNDPAT) or a signal (the C input).\n");
		log("Products go two to an ALU, a negative one in the subtracting slot; with\n");
		log("more than one ALU the fabric adds their partial sums after the registers.\n");
		log("Operand flops on the same clock move into the MULT18X18D input registers.\n");
		log("\n");
		log("    -tiles\n");
		log("        run after mul2dsp has split a wider multiply into $__MUL18X18 tiles\n");
		log("        and an adder tree: move a register on the product back across the\n");
		log("        tree onto every tile's output register, and copy registers on the\n");
		log("        operands into every tile's input registers.\n");
		log("\n");
		log("    -dsp-shift\n");
		log("        map a variable left shift of a value up to 18 bits by 2 to 4 bits\n");
		log("        onto a MULT18X18D multiply by the one-hot 1 << S, instead of a LUT\n");
		log("        barrel shifter. It takes a DSP block for each shift and saves LUTs,\n");
		log("        but the multiplier array is slower than a few LUT levels: expect\n");
		log("        lower Fmax.\n");
		log("\n");
		log("    -mac1\n");
		log("        also give a running sum of a single product an ALU54B. nextpnr\n");
		log("        needs both MULT18X18D of the pair on the ALU, so the other one idles.\n");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing LATTICE_DSP_ECP5 pass.\n");
		bool tiles = false, dsp_shift = false, mac1 = false;
		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			if (args[argidx] == "-tiles") {
				tiles = true;
				continue;
			}
			if (args[argidx] == "-dsp-shift") {
				dsp_shift = true;
				continue;
			}
			if (args[argidx] == "-mac1") {
				mac1 = true;
				continue;
			}
			break;
		}
		extra_args(args, argidx, design);

		for (auto module : design->selected_modules()) {
			if (tiles) {
				retime_split_muls(module);
				continue;
			}
			// ECP5 has no dot-product cell: a dot stays an adder tree of
			// products, and only this shows the tree's true width.
			pool<IdString> exact_sums = bound_arith(module);
			fold_scale_into_mul(module, true);
			if (dsp_shift)
				shift_to_mul(module);
			narrow_mul_operands(module);
			infer_ecp5_macs(module, mac1, exact_sums);
			lattice_dsp_nexus_pm pm(module, module->cells());
			pm.ud_nexus_mul_reg.ecp5 = true;
			pm.run_nexus_mul_reg();
		}
	}
} LatticeDspEcp5Pass;

struct LatticeBramEcp5Pass : public Pass {
	LatticeBramEcp5Pass() : Pass("lattice_bram_ecp5", "Lattice ECP5 block RAM output registers") { }
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    lattice_bram_ecp5 [selection]\n");
		log("\n");
		log("Move a register on the read data of a DP16KD port into that port's output\n");
		log("register (REGMODE_A/B = OUTREG, enabled by OCEA/OCEB). The register must\n");
		log("have no reset, share the port's clock, and be the only reader of the read\n");
		log("data; the port's RST must be tied low. The 36-bit mode is left alone.\n");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing LATTICE_BRAM_ECP5 pass.\n");
		extra_args(args, 1, design);
		for (auto module : design->selected_modules())
			absorb_bram_outregs(module);
	}
} LatticeBramEcp5Pass;

struct LatticeIoffEcp5Pass : public Pass {
	LatticeIoffEcp5Pass() : Pass("lattice_ioff_ecp5", "Lattice ECP5 IO register marking") { }
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    lattice_ioff_ecp5 [selection]\n");
		log("\n");
		log("Set syn_useioff on TRELLIS_FF cells that nextpnr can place in a pin's IOLOGIC:\n");
		log("an input flop whose DI is a top-level input bit read by nothing else, and an\n");
		log("output flop whose Q drives a top-level output bit and nothing else. nextpnr\n");
		log("only packs IO registers on LOC-constrained pins and stops with an error on\n");
		log("unconstrained ones, so run this only when every such pin is constrained.\n");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing LATTICE_IOFF_ECP5 pass.\n");
		extra_args(args, 1, design);
		for (auto module : design->selected_modules())
			mark_io_ffs(module);
	}
} LatticeIoffEcp5Pass;

PRIVATE_NAMESPACE_END
