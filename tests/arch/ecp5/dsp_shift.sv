// -dsp-shift: a small variable left shift becomes a multiply by a one-hot.

module shift_u (input [17:0] x, input [3:0] sh, output [33:0] y);
	assign y = x << sh;
endmodule

module shift_s (input signed [15:0] x, input [3:0] sh, output signed [31:0] y);
	assign y = x <<< sh;
endmodule

// A constant shift is wiring, never a multiply.
module shift_const (input [17:0] x, output [33:0] y);
	assign y = x << 4'd5;
endmodule

// Too wide for one MULT18X18D port.
module shift_wide (input [23:0] x, input [3:0] sh, output [39:0] y);
	assign y = x << sh;
endmodule
