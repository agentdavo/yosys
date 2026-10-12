// Behaviour of $__ECP5_MAC54 (techlibs/lattice/dsp_map_18x18.v), for proofs.
module \$__ECP5_MAC54 (input CLK, CE, RST, LOADN, CEA0, RSTA0, CEB0, RSTB0, CEP0, RSTP0,
		CEA1, RSTA1, CEB1, RSTB1, CEP1, RSTP1,
		input [17:0] A0, B0, A1, B1, input [53:0] C, output [Y_WIDTH-1:0] Y);
	parameter Y_WIDTH = 54;
	parameter SIGNED0 = 0;
	parameter SIGNED1 = 0;
	parameter ASEL = 1;
	parameter BSEL = 1;
	parameter BSUB = 0;
	parameter CSEL = 1;
	parameter CSUB = 0;
	parameter RNDPAT = "0x00000000000000";
	parameter RNDVAL = 54'd0;
	parameter REGIN0 = 0;
	parameter REGIN1 = 0;
	parameter PIPE0 = 0;
	parameter PIPE1 = 0;
	reg [17:0] a0r = 0, b0r = 0, a1r = 0, b1r = 0;
	always @(posedge CLK) begin
		if (RSTA0) a0r <= 0; else if (CEA0) a0r <= A0;
		if (RSTB0) b0r <= 0; else if (CEB0) b0r <= B0;
		if (RSTA1) a1r <= 0; else if (CEA1) a1r <= A1;
		if (RSTB1) b1r <= 0; else if (CEB1) b1r <= B1;
	end
	wire [17:0] a0 = REGIN0 ? a0r : A0, b0 = REGIN0 ? b0r : B0;
	wire [17:0] a1 = REGIN1 ? a1r : A1, b1 = REGIN1 ? b1r : B1;
	wire [53:0] m0 = SIGNED0 ? $signed(a0) * $signed(b0) : a0 * b0;
	wire [53:0] m1 = SIGNED1 ? $signed(a1) * $signed(b1) : a1 * b1;
	reg [53:0] m0r = 0, m1r = 0;
	always @(posedge CLK) begin
		if (RSTP0) m0r <= 0; else if (CEP0) m0r <= m0;
		if (RSTP1) m1r <= 0; else if (CEP1) m1r <= m1;
	end
	wire [53:0] p0 = !ASEL ? 54'd0 : PIPE0 ? m0r : m0;
	wire [53:0] p1 = !BSEL ? 54'd0 : PIPE1 ? m1r : m1;
	reg [53:0] r = 0;
	wire [53:0] c = CSEL == 1 ? (LOADN ? r : 54'd0) : CSEL == 2 ? C : CSEL == 3 ? RNDVAL : 54'd0;
	always @(posedge CLK)
		if (RST) r <= 0;
		else if (CE) r <= p0 + (BSUB ? -p1 : p1) + (CSUB ? -c : c);
	assign Y = r[Y_WIDTH-1:0];
endmodule
