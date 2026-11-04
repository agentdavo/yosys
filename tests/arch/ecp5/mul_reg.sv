// Flops around an 18x18 multiply move into the MULT18X18D registers.

module mul18_reg (input clk, input [17:0] a, b, output reg [35:0] y);
	reg [17:0] a_r, b_r;
	always @(posedge clk) begin
		a_r <= a;
		b_r <= b;
		y <= a_r * b_r;
	end
endmodule

// Separate enables and a reset: each register keeps its own CE/RST pin.
module mul18_ctrl (input clk, rst, ea, eb, eo, input signed [17:0] a, b, output reg signed [35:0] y);
	reg signed [17:0] a_r, b_r;
	always @(posedge clk) begin
		if (ea)
			a_r <= a;
		if (rst)
			b_r <= 0;
		else if (eb)
			b_r <= b;
		if (!eo)
			y <= a_r * b_r;
	end
endmodule

// One scale register read by two multiplies: each DSP takes a copy.
module shared_scale (input clk, input [15:0] d0, d1, input [7:0] scale, output reg [23:0] y0, y1);
	reg [7:0] scale_r;
	always @(posedge clk) begin
		scale_r <= scale;
		y0 <= d0 * scale_r;
		y1 <= d1 * scale_r;
	end
endmodule

// The product is also read combinationally, so its register stays outside.
module prod_used (input clk, input [15:0] a, b, output reg [31:0] y, output [31:0] z);
	assign z = a * b;
	always @(posedge clk)
		y <= z;
endmodule

// Split across four MULT18X18D tiles.
module mul32_reg (input clk, input [31:0] a, b, output reg [63:0] y);
	reg [31:0] a_r, b_r;
	always @(posedge clk) begin
		a_r <= a;
		b_r <= b;
		y <= a_r * b_r;
	end
endmodule

module mul32_signed (input clk, input signed [31:0] a, b, output reg signed [63:0] y);
	reg signed [31:0] a_r, b_r;
	always @(posedge clk) begin
		a_r <= a;
		b_r <= b;
		y <= a_r * b_r;
	end
endmodule

module mul32_seen (input clk, input [31:0] a, b, output reg [63:0] y, output [63:0] z);
	reg [31:0] a_r, b_r;
	assign z = a_r * b_r;
	always @(posedge clk) begin
		a_r <= a;
		b_r <= b;
		y <= z;
	end
endmodule

module mul18_comb (input [17:0] a, b, output [35:0] y);
	assign y = a * b;
endmodule
