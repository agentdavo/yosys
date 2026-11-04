// Scale after a 4-term int8 dot: the dot is declared 20 bits but only
// reaches 18, so the scale multiply stays one MULT18X18D.
module scale_after_dot (input [7:0] a0, b0, a1, b1, a2, b2, a3, b3, input [7:0] scale, output [27:0] y);
	wire [19:0] dot = a0*b0 + a1*b1 + a2*b2 + a3*b3;
	assign y = dot * scale;
endmodule

module shift_after_dot (input [7:0] a0, b0, a1, b1, a2, b2, a3, b3, input [3:0] sh, output [35:0] y);
	wire [19:0] dot = a0*b0 + a1*b1 + a2*b2 + a3*b3;
	assign y = dot << sh;
endmodule

// A truncated copy must not inherit the untruncated range.
module trunc (input [7:0] a, b, c, d, input [9:0] e, output [19:0] y, output [19:0] z);
	wire [7:0] p = a * b;
	wire [8:0] s = c * d + p;
	assign y = p * e;
	assign z = s * e;
endmodule

// Products past 2^62 must be left alone, not clamped.
module wide (input [31:0] a, b, output [63:0] y);
	assign y = a * b;
endmodule
