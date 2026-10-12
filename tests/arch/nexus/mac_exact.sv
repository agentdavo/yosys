// The DSP post-adder adds the exact product, extended by its own
// signedness. An RTL product that is truncated, or extended the other way,
// is a different value and must not fold into the DSP adder.

module mac_trunc (input [7:0] a, b, input [19:0] c, output [19:0] y);
	wire [7:0] m = a * b;
	assign y = c + m;
endmodule

module mac_signed_zext (input signed [7:0] a, b, input [19:0] c, output [19:0] y);
	wire signed [15:0] m = a * b;
	assign y = c + $unsigned(m);
endmodule

module mac36_trunc (input [23:0] a, b, input [40:0] c, output [40:0] y);
	wire [29:0] m = a * b;
	assign y = c + m;
endmodule

module dot4_trunc_prod (input [7:0] a0, b0, a1, b1, a2, b2, a3, b3, output [19:0] y);
	wire [9:0] m0 = a0*b0;
	assign y = m0 + a1*b1 + a2*b2 + a3*b3;
endmodule

module dot4_trunc_sum (input [7:0] a0, b0, a1, b1, a2, b2, a3, b3, output [19:0] y);
	wire [15:0] s = a0*b0 + a1*b1;
	assign y = s + a2*b2 + a3*b3;
endmodule

module acc_trunc (input clk, input [7:0] a, b, output reg [19:0] p);
	wire [7:0] m = a * b;
	always @(posedge clk)
		p <= p + m;
endmodule

// A product wider than the sum is exact where it matters: still fuses.
module mac_wide_prod (input [7:0] a, b, input [11:0] c, output [11:0] y);
	assign y = c + a * b;
endmodule
