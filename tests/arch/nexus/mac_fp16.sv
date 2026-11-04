// A floating-point block scale after an int8 dot, d = (-1)^s * 1.m * 2^e.
// The sign and the low exponent bits move from the wide product onto the
// narrow mantissa operand of the multiply.

// fp16 scale: two of the five shift levels fit the 18-bit operand.
module dot_fp16_scale (input clk, input signed [7:0] a0, b0, a1, b1, a2, b2, a3, b3,
		input [15:0] d, output reg signed [63:0] y);
	wire signed [17:0] dot = a0*b0 + a1*b1 + a2*b2 + a3*b3;
	wire [4:0] e = d[14:10];
	wire [10:0] m = {e != 5'd0, d[9:0]};
	wire [4:0] sh = (e == 5'd0) ? 5'd1 : e;
	wire signed [29:0] p = dot * $signed({1'b0, m});
	wire signed [63:0] scaled = p <<< sh;
	always @(posedge clk)
		y <= d[15] ? -scaled : scaled;
endmodule

// Minifloat scale accumulated over blocks: the whole shift and the sign fit,
// so the scaled product fuses with the accumulator in one DSP.
module minifloat_acc (input clk, rst, input signed [2:0] a0, b0, a1, b1, a2, b2, a3, b3,
		input [6:0] d, output reg signed [27:0] acc);
	wire signed [7:0] dot = a0*b0 + a1*b1 + a2*b2 + a3*b3;
	wire [2:0] e = d[5:3];
	wire [3:0] m = {e != 3'd0, d[2:0]};
	wire signed [12:0] p = dot * $signed({1'b0, m});
	wire signed [23:0] scaled = p <<< e;
	always @(posedge clk)
		if (rst)
			acc <= 0;
		else
			acc <= acc + (d[6] ? -scaled : scaled);
endmodule

// Must keep the wide negate: scaled can be its own most negative value, so
// negating after the extension is not negating the product.
module no_headroom (input signed [3:0] x, w, input [1:0] sh, input s, output signed [15:0] y);
	wire signed [7:0] p = x * w;
	wire signed [7:0] scaled = p <<< sh;
	assign y = s ? -scaled : scaled;
endmodule

// Must not fold: the product is read directly as well.
module prod_seen (input signed [5:0] x, input signed [3:0] w, input s, output signed [9:0] y, p);
	assign p = x * w;
	assign y = s ? -p : p;
endmodule
