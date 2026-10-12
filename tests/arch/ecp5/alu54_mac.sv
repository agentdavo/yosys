// Running sums of 18x18 products for ALU54B inference.
module mac2 (input clk, input [7:0] a0, b0, a1, b1, output reg [31:0] acc);
	always @(posedge clk) acc <= acc + a0*b0 + a1*b1;
endmodule

module mac2_signed (input clk, input signed [17:0] a0, b0, a1, b1, output reg signed [51:0] acc);
	always @(posedge clk) acc <= acc + a0*b0 + a1*b1;
endmodule

module mac_load (input clk, clr, input signed [7:0] a0, b0, a1, b1, output reg signed [31:0] acc);
	always @(posedge clk)
		if (clr) acc <= a0*b0 + a1*b1;
		else acc <= acc + a0*b0 + a1*b1;
endmodule

module mac1_en_rst (input clk, en, rst, input signed [15:0] a, b, output reg signed [47:0] acc);
	always @(posedge clk)
		if (rst) acc <= 0;
		else if (en) acc <= acc + a*b;
endmodule

// Small enough for SAT: the pass alone against the RTL
module mac_small (input clk, clr, en, input signed [3:0] a0, b0, input [3:0] a1, b1, output reg [11:0] acc);
	always @(posedge clk)
		if (en) begin
			if (clr) acc <= a0*b0 + a1*b1;
			else acc <= acc + a0*b0 + a1*b1;
		end
endmodule

// More products than one ALU54B: an ALU per pair, the partial sums added
// after the registers
module dot4 (input clk, input signed [7:0] a0, b0, a1, b1, a2, b2, a3, b3, output reg signed [31:0] acc);
	always @(posedge clk) acc <= acc + a0*b0 + a1*b1 + a2*b2 + a3*b3;
endmodule

// A rotation: sum and difference registers, no feedback
module rope (input clk, input signed [15:0] x0, x1, c, s, output reg signed [32:0] y0, y1);
	always @(posedge clk) begin
		y0 <= x0*c - x1*s;
		y1 <= x0*s + x1*c;
	end
endmodule

// A signal addend on the C input, a constant one in RNDPAT
module dot2_bias (input clk, input signed [7:0] a0, b0, a1, b1, input signed [19:0] bias, output reg signed [23:0] y);
	always @(posedge clk) y <= a0*b0 + a1*b1 + bias;
endmodule

module dot2_round (input clk, input signed [15:0] a0, b0, a1, b1, output reg signed [33:0] y);
	always @(posedge clk) y <= a0*b0 + a1*b1 + 34'sd32768;
endmodule

// Operand flops on the ALU's clock move into the MULT18X18D input registers,
// both operands of a multiplier or neither
module regin (input clk, input signed [7:0] a0, b0, a1, b1, output reg signed [31:0] acc);
	reg signed [7:0] a0_r, b0_r, a1_r, b1_r;
	always @(posedge clk) begin
		a0_r <= a0; b0_r <= b0; a1_r <= a1; b1_r <= b1;
		acc <= acc + a0_r*b0_r + a1_r*b1_r;
	end
endmodule

module regin_half (input clk, input signed [7:0] a0, b0, a1, b1, output reg signed [31:0] acc);
	reg signed [7:0] a0_r;
	always @(posedge clk) begin
		a0_r <= a0;
		acc <= acc + a0_r*b0 + a1*b1;
	end
endmodule

// A register on a product becomes the MULT18X18D pipeline register, unless
// something else reads it
module pipe_full (input clk, clr, input signed [7:0] a0, b0, a1, b1, output reg signed [31:0] acc);
	reg signed [7:0] a0_r, b0_r, a1_r, b1_r;
	reg signed [15:0] p0, p1;
	always @(posedge clk) begin
		a0_r <= a0; b0_r <= b0; a1_r <= a1; b1_r <= b1;
		p0 <= a0_r*b0_r; p1 <= a1_r*b1_r;
		acc <= clr ? p0 + p1 : acc + p0 + p1;
	end
endmodule

module pipe_shared (input clk, input signed [7:0] a0, b0, a1, b1, output reg signed [31:0] acc, output signed [15:0] tap);
	reg signed [15:0] p0, p1;
	assign tap = p0;
	always @(posedge clk) begin
		p0 <= a0*b0; p1 <= a1*b1;
		acc <= acc + p0 + p1;
	end
endmodule

// Small enough for SAT
module pipe_small (input clk, en, input signed [3:0] a0, b0, a1, b1, output reg signed [11:0] acc);
	reg signed [7:0] p0, p1;
	always @(posedge clk) begin
		if (en) p0 <= a0*b0;
		p1 <= a1*b1;
		acc <= acc + p0 - p1;
	end
endmodule

module regin_small (input clk, en, input signed [3:0] a0, b0, a1, b1, output reg [11:0] acc);
	reg signed [3:0] a0_r, b0_r, a1_r, b1_r;
	always @(posedge clk) begin
		if (en) begin a0_r <= a0; b0_r <= b0; end
		a1_r <= a1; b1_r <= b1;
		acc <= acc + a0_r*b0_r + a1_r*b1_r;
	end
endmodule

module sum_small (input clk, input signed [3:0] a0, b0, a1, b1, a2, b2, a3, b3, input signed [5:0] c, output reg [11:0] y);
	always @(posedge clk) y <= a0*b0 - a1*b1 + a2*b2 + a3*b3 + c;
endmodule

module acc4_small (input clk, clr, input signed [3:0] a0, b0, a1, b1, a2, b2, a3, b3, output reg [13:0] acc);
	wire signed [10:0] s = a0*b0 + a1*b1 - a2*b2 + a3*b3;
	always @(posedge clk) acc <= clr ? {{3{s[10]}}, s} : acc + {{3{s[10]}}, s};
endmodule

module round_small (input clk, input [3:0] a0, b0, a1, b1, output reg [9:0] y);
	always @(posedge clk) y <= a0*b0 + a1*b1 + 10'd5;
endmodule

// rejects
// An unsigned difference that wraps below zero is not the true sum
module rej_wrapdiff (input clk, input [3:0] a0, b0, a1, b1, a2, b2, a3, b3, output reg [13:0] acc);
	wire [9:0] s = a0*b0 + a1*b1 - a2*b2 + a3*b3;
	always @(posedge clk) acc <= acc + {{4{s[9]}}, s};
endmodule

module rej_trunc (input clk, input [7:0] a, b, output reg [23:0] acc);
	wire [11:0] p = a*b;
	always @(posedge clk) acc <= acc + p;
endmodule

module rej_shared (input clk, input [7:0] a, b, output reg [23:0] acc, output [15:0] p);
	assign p = a*b;
	always @(posedge clk) acc <= acc + p;
endmodule

module rej_wide (input clk, input [17:0] a, b, output reg [59:0] acc);
	always @(posedge clk) acc <= acc + a*b;
endmodule

module rej_twice (input clk, input [7:0] a, b, output reg [23:0] acc);
	wire [15:0] p = a*b;
	always @(posedge clk) acc <= acc + p + p;
endmodule

// The data sheet promises a 52-bit accumulator: a 53-bit one stays in the fabric
module rej_53 (input clk, input signed [17:0] a0, b0, a1, b1, output reg signed [52:0] acc);
	always @(posedge clk) acc <= acc + a0*b0 + a1*b1;
endmodule
