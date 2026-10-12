// Running accumulators: p <= p +/- A*B closes inside the DSP. The output
// register is the accumulator and LOADC=0 feeds it back into ACC54.

module running_acc (input clk, input [7:0] a, b, output reg [19:0] p);
	always @(posedge clk)
		p <= p + a * b;
endmodule

module acc_signed (input clk, input signed [7:0] a, b, output reg signed [19:0] p);
	always @(posedge clk)
		p <= p + a * b;
endmodule

module acc_sub (input clk, input [7:0] a, b, output reg [19:0] p);
	always @(posedge clk)
		p <= p - a * b;
endmodule

module acc_rst_en (input clk, rst, en, input [7:0] a, b, output reg [19:0] p);
	always @(posedge clk)
		if (rst)
			p <= 0;
		else if (en)
			p <= p + a * b;
endmodule

// Four int8 lanes into one accumulator -> MULTADDSUB9X9WIDE
module acc_dot4 (input clk, rst, input signed [7:0] a0, b0, a1, b1, a2, b2, a3, b3, output reg signed [23:0] p);
	always @(posedge clk)
		if (rst)
			p <= 0;
		else
			p <= p + a0*b0 + a1*b1 + a2*b2 + a3*b3;
endmodule

// The sum is also read combinationally, so it must stay outside the DSP.
module acc_sum_used (input clk, input [7:0] a, b, output [19:0] y, output reg [19:0] p);
	assign y = p + a * b;
	always @(posedge clk)
		p <= y;
endmodule

// The DSP output register can only reset to zero.
module acc_rst_one (input clk, rst, input [7:0] a, b, output reg [19:0] p);
	always @(posedge clk)
		if (rst)
			p <= 20'd1;
		else
			p <= p + a * b;
endmodule

// The addend is a different register, not the fed-back sum.
module acc_other_reg (input clk, input [7:0] a, b, input [19:0] d, output reg [19:0] p);
	reg [19:0] q;
	always @(posedge clk) begin
		q <= d;
		p <= q + a * b;
	end
endmodule
