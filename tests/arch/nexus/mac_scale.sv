// A 4-lane dot followed by an integer scale (Q6_K-style group scale).
// Four 8x8 products sum to 18 bits however wide the dot is declared, so the
// scale multiply fits one MULT18X18 with its own input and output registers.

module scale_after_dot (input [7:0] a0, b0, a1, b1, a2, b2, a3, b3, input [7:0] scale, output [27:0] y);
	wire [19:0] dot = a0*b0 + a1*b1 + a2*b2 + a3*b3;
	assign y = dot * scale;
endmodule

module scale_after_dot_reg (input clk, input [7:0] a0, b0, a1, b1, a2, b2, a3, b3, input [7:0] scale, output reg [27:0] y);
	reg [7:0] scale_r;
	wire [19:0] dot = a0*b0 + a1*b1 + a2*b2 + a3*b3;
	always @(posedge clk) begin
		scale_r <= scale;
		y <= dot * scale_r;
	end
endmodule

module scale_after_dot_signed (input clk, input signed [7:0] a0, b0, a1, b1, a2, b2, a3, b3, input signed [7:0] scale, output reg signed [27:0] y);
	reg signed [7:0] scale_r;
	wire signed [19:0] dot = a0*b0 + a1*b1 + a2*b2 + a3*b3;
	always @(posedge clk) begin
		scale_r <= scale;
		y <= dot * scale_r;
	end
endmodule

// Q6_K group with its operands held in registers. The lane flops become
// the 4-lane cell's REGINPUTABn and the scale flop the MULT18X18's B
// register: nothing is left in the fabric.
module q6k_group (input clk, input signed [7:0] a0, b0, a1, b1, a2, b2, a3, b3, input signed [7:0] scale, output reg signed [27:0] y);
	reg signed [7:0] a0_r, b0_r, a1_r, b1_r, a2_r, b2_r, a3_r, b3_r, scale_r;
	wire signed [19:0] dot = a0_r*b0_r + a1_r*b1_r + a2_r*b2_r + a3_r*b3_r;
	always @(posedge clk) begin
		{a0_r, b0_r, a1_r, b1_r, a2_r, b2_r, a3_r, b3_r} <= {a0, b0, a1, b1, a2, b2, a3, b3};
		scale_r <= scale;
		y <= dot * scale_r;
	end
endmodule

// Lanes 0 and 1 share one A and one B enable in the DSP. Different enables
// mean only lane 0 can take its registers.
module lanes_split_en (input clk, e0, e1, input [3:0] a0, b0, a1, b1, a2, b2, a3, b3, output [11:0] y);
	reg [3:0] a0_r, b0_r, a1_r, b1_r;
	always @(posedge clk) begin
		if (e0) begin
			a0_r <= a0;
			b0_r <= b0;
		end
		if (e1) begin
			a1_r <= a1;
			b1_r <= b1;
		end
	end
	assign y = a0_r*b0_r + a1_r*b1_r + a2*b2 + a3*b3;
endmodule

// One scale register read by two multiplies: each DSP takes a copy.
module shared_scale (input clk, input [5:0] d0, d1, input [3:0] scale, output reg [9:0] y0, y1);
	reg [3:0] scale_r;
	always @(posedge clk) begin
		scale_r <= scale;
		y0 <= d0 * scale_r;
		y1 <= d1 * scale_r;
	end
endmodule

// The product is also read combinationally, so its register stays outside.
module shared_prod (input clk, input [15:0] a, b, output reg [31:0] y, output [31:0] z);
	assign z = a * b;
	always @(posedge clk)
		y <= z;
endmodule
