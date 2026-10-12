// Pin-to-register and register-to-pin paths around a small datapath.
module io (input clk, input [7:0] a, b, c, output reg [7:0] y, output reg [7:0] w, output [7:0] z);
	reg [7:0] a_r, b_r, c_r;
	always @(posedge clk) begin
		a_r <= a;            // input flops: DI is the pin
		b_r <= b;
		c_r <= c;
		y <= a_r + b_r;      // y is also read below, so it stays in the fabric
		w <= a_r - c_r;      // output flop: Q drives only the pin
	end
	assign z = c_r ^ y;
endmodule
