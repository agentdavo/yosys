// A register on block RAM read data moves into the DP16KD output register.

module outreg (input clk, we, input [9:0] wa, ra, input [15:0] wd, output reg [15:0] y);
	(* no_rw_check *) reg [15:0] mem [0:1023];
	reg [15:0] w;
	always @(posedge clk) begin
		if (we)
			mem[wa] <= wd;
		w <= mem[ra];
		y <= w;
	end
endmodule

// The register's enable becomes OCE.
module outreg_en (input clk, we, en, input [9:0] wa, ra, input [15:0] wd, output reg [15:0] y);
	(* no_rw_check *) reg [15:0] mem [0:1023];
	reg [15:0] w;
	always @(posedge clk) begin
		if (we)
			mem[wa] <= wd;
		w <= mem[ra];
		if (en)
			y <= w;
	end
endmodule

// How RST acts on the output register is not modelled: keep it outside.
module outreg_rst (input clk, we, rst, input [9:0] wa, ra, input [15:0] wd, output reg [15:0] y);
	(* no_rw_check *) reg [15:0] mem [0:1023];
	reg [15:0] w;
	always @(posedge clk) begin
		if (we)
			mem[wa] <= wd;
		w <= mem[ra];
		if (rst)
			y <= 0;
		else
			y <= w;
	end
endmodule

// The read data is also used directly.
module outreg_seen (input clk, we, input [9:0] wa, ra, input [15:0] wd, output reg [15:0] y, output [15:0] z);
	(* no_rw_check *) reg [15:0] mem [0:1023];
	reg [15:0] w;
	assign z = w;
	always @(posedge clk) begin
		if (we)
			mem[wa] <= wd;
		w <= mem[ra];
		y <= w;
	end
endmodule

// The 36-bit mode spreads read data over both ports: left alone.
module outreg_wide (input clk, we, input [8:0] wa, ra, input [35:0] wd, output reg [35:0] y);
	(* no_rw_check *) reg [35:0] mem [0:511];
	reg [35:0] w;
	always @(posedge clk) begin
		if (we)
			mem[wa] <= wd;
		w <= mem[ra];
		y <= w;
	end
endmodule
