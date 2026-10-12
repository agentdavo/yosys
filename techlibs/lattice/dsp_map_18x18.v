module \$__MUL18X18 (input [17:0] A, input [17:0] B, input CLK, CEA, RSTA, CEB, RSTB, CEOUT, RSTOUT, output [35:0] Y);

	parameter A_WIDTH = 18;
	parameter B_WIDTH = 18;
	parameter Y_WIDTH = 36;
	parameter A_SIGNED = 0;
	parameter B_SIGNED = 0;
	// Set by lattice_dsp_ecp5 when it moves fabric flops into the DSP.
	parameter REGINPUTA = "BYPASS";
	parameter REGINPUTB = "BYPASS";
	parameter REGOUTPUT = "BYPASS";
	parameter RESETMODE = "SYNC";

	// A on CE0/RST0, B on CE1/RST1, the product on CE2/RST2, all on CLK0,
	// so each register keeps its own enable and reset.
	generate if (REGINPUTA == "REGISTER" || REGINPUTB == "REGISTER" || REGOUTPUT == "REGISTER") begin
	MULT18X18D #(
		.REG_INPUTA_CLK(REGINPUTA == "REGISTER" ? "CLK0" : "NONE"), .REG_INPUTA_CE("CE0"), .REG_INPUTA_RST("RST0"),
		.REG_INPUTB_CLK(REGINPUTB == "REGISTER" ? "CLK0" : "NONE"), .REG_INPUTB_CE("CE1"), .REG_INPUTB_RST("RST1"),
		.REG_OUTPUT_CLK(REGOUTPUT == "REGISTER" ? "CLK0" : "NONE"), .REG_OUTPUT_CE("CE2"), .REG_OUTPUT_RST("RST2"),
		.RESETMODE(RESETMODE)
	) _TECHMAP_REPLACE_ (
		.A0(A[0]), .A1(A[1]), .A2(A[2]), .A3(A[3]), .A4(A[4]), .A5(A[5]), .A6(A[6]), .A7(A[7]), .A8(A[8]), .A9(A[9]), .A10(A[10]), .A11(A[11]), .A12(A[12]), .A13(A[13]), .A14(A[14]), .A15(A[15]), .A16(A[16]), .A17(A[17]),
		.B0(B[0]), .B1(B[1]), .B2(B[2]), .B3(B[3]), .B4(B[4]), .B5(B[5]), .B6(B[6]), .B7(B[7]), .B8(B[8]), .B9(B[9]), .B10(B[10]), .B11(B[11]), .B12(B[12]), .B13(B[13]), .B14(B[14]), .B15(B[15]), .B16(B[16]), .B17(B[17]),
		.C17(1'b0), .C16(1'b0), .C15(1'b0), .C14(1'b0), .C13(1'b0), .C12(1'b0), .C11(1'b0), .C10(1'b0), .C9(1'b0), .C8(1'b0), .C7(1'b0), .C6(1'b0), .C5(1'b0), .C4(1'b0), .C3(1'b0), .C2(1'b0), .C1(1'b0), .C0(1'b0),
		.SIGNEDA(A_SIGNED ? 1'b1 : 1'b0), .SIGNEDB(B_SIGNED ? 1'b1 : 1'b0), .SOURCEA(1'b0), .SOURCEB(1'b0),
		.CLK0(CLK), .CE0(CEA), .RST0(RSTA), .CE1(CEB), .RST1(RSTB), .CE2(CEOUT), .RST2(RSTOUT),

		.P0(Y[0]), .P1(Y[1]), .P2(Y[2]), .P3(Y[3]), .P4(Y[4]), .P5(Y[5]), .P6(Y[6]), .P7(Y[7]), .P8(Y[8]), .P9(Y[9]), .P10(Y[10]), .P11(Y[11]), .P12(Y[12]), .P13(Y[13]), .P14(Y[14]), .P15(Y[15]), .P16(Y[16]), .P17(Y[17]), .P18(Y[18]), .P19(Y[19]), .P20(Y[20]), .P21(Y[21]), .P22(Y[22]), .P23(Y[23]), .P24(Y[24]), .P25(Y[25]), .P26(Y[26]), .P27(Y[27]), .P28(Y[28]), .P29(Y[29]), .P30(Y[30]), .P31(Y[31]), .P32(Y[32]), .P33(Y[33]), .P34(Y[34]), .P35(Y[35])
	);
	end else begin
	MULT18X18D _TECHMAP_REPLACE_ (
		.A0(A[0]), .A1(A[1]), .A2(A[2]), .A3(A[3]), .A4(A[4]), .A5(A[5]), .A6(A[6]), .A7(A[7]), .A8(A[8]), .A9(A[9]), .A10(A[10]), .A11(A[11]), .A12(A[12]), .A13(A[13]), .A14(A[14]), .A15(A[15]), .A16(A[16]), .A17(A[17]),
		.B0(B[0]), .B1(B[1]), .B2(B[2]), .B3(B[3]), .B4(B[4]), .B5(B[5]), .B6(B[6]), .B7(B[7]), .B8(B[8]), .B9(B[9]), .B10(B[10]), .B11(B[11]), .B12(B[12]), .B13(B[13]), .B14(B[14]), .B15(B[15]), .B16(B[16]), .B17(B[17]),
		.C17(1'b0), .C16(1'b0), .C15(1'b0), .C14(1'b0), .C13(1'b0), .C12(1'b0), .C11(1'b0), .C10(1'b0), .C9(1'b0), .C8(1'b0), .C7(1'b0), .C6(1'b0), .C5(1'b0), .C4(1'b0), .C3(1'b0), .C2(1'b0), .C1(1'b0), .C0(1'b0),
		.SIGNEDA(A_SIGNED ? 1'b1 : 1'b0), .SIGNEDB(B_SIGNED ? 1'b1 : 1'b0), .SOURCEA(1'b0), .SOURCEB(1'b0),

		.P0(Y[0]), .P1(Y[1]), .P2(Y[2]), .P3(Y[3]), .P4(Y[4]), .P5(Y[5]), .P6(Y[6]), .P7(Y[7]), .P8(Y[8]), .P9(Y[9]), .P10(Y[10]), .P11(Y[11]), .P12(Y[12]), .P13(Y[13]), .P14(Y[14]), .P15(Y[15]), .P16(Y[16]), .P17(Y[17]), .P18(Y[18]), .P19(Y[19]), .P20(Y[20]), .P21(Y[21]), .P22(Y[22]), .P23(Y[23]), .P24(Y[24]), .P25(Y[25]), .P26(Y[26]), .P27(Y[27]), .P28(Y[28]), .P29(Y[29]), .P30(Y[30]), .P31(Y[31]), .P32(Y[32]), .P33(Y[33]), .P34(Y[34]), .P35(Y[35])
	);
	end endgenerate
endmodule

// A sum of up to two 18x18 products, from lattice_dsp_ecp5: two MULT18X18D
// (nextpnr needs both, feeding MA and MB) and an ALU54B whose output
// register holds the result. The opcode, from Lattice's ALU54B simulation
// model:
//   OP[1:0]   01 MA, 11 zero                       (ASEL)
//   OP[3:2]   01 MB, 11 zero                       (BSEL)
//   OP[6:4]   000 zero, 101 the registered result R, 011 the C input,
//             110 RNDPAT                           (CSEL 0, 1, 2, 3)
//   OP[10:7]  01 CSUB BSUB: A + B + C, A - B + C, A + B - C, A - B - C
// With CSEL 1, LOADN low selects zero instead of R: a reload. C stays
// unconnected unless it is used, and SIGNEDCIN always does: nextpnr routes a
// constant SIGNEDCIN like the cascade input it is, and that route fails.
// REGINk puts the operands of MULT18X18D k
// in its input registers, on the ALU's clock: A on CEAk/RSTAk, B on CEBk/RSTBk.
// PIPEk puts its product in its pipeline register, on CEPk/RSTPk.
module \$__ECP5_MAC54 (input CLK, CE, RST, LOADN, CEA0, RSTA0, CEB0, RSTB0, CEP0, RSTP0,
		CEA1, RSTA1, CEB1, RSTB1, CEP1, RSTP1,
		input [17:0] A0, B0, A1, B1, input [53:0] C, output [Y_WIDTH-1:0] Y);

	parameter Y_WIDTH = 54;
	parameter SIGNED0 = 0;
	parameter SIGNED1 = 0;
	parameter REGIN0 = 0;
	parameter REGIN1 = 0;
	parameter PIPE0 = 0;
	parameter PIPE1 = 0;
	parameter ASEL = 1;
	parameter BSEL = 1;
	parameter BSUB = 0;
	parameter CSEL = 1;
	parameter CSUB = 0;
	parameter RNDPAT = "0x00000000000000";
	parameter RNDVAL = 54'd0;

	wire [35:0] p0, p1;
	wire [17:0] ra0, rb0, ra1, rb1;
	wire s0, s1;
	wire [2:0] csel = CSEL == 1 ? {LOADN, 1'b0, LOADN} : CSEL == 2 ? 3'b011 : CSEL == 3 ? 3'b110 : 3'b000;
	wire [10:0] op = {2'b01, CSUB ? 1'b1 : 1'b0, BSUB ? 1'b1 : 1'b0, csel, BSEL ? 2'b01 : 2'b11, ASEL ? 2'b01 : 2'b11};
	wire [53:0] r;
	assign Y = r[Y_WIDTH-1:0];

	generate if (REGIN0 || PIPE0) begin
		MULT18X18D #(
			.REG_INPUTA_CLK(REGIN0 ? "CLK0" : "NONE"), .REG_INPUTA_CE("CE0"), .REG_INPUTA_RST("RST0"),
			.REG_INPUTB_CLK(REGIN0 ? "CLK0" : "NONE"), .REG_INPUTB_CE("CE1"), .REG_INPUTB_RST("RST1"),
			.REG_PIPELINE_CLK(PIPE0 ? "CLK0" : "NONE"), .REG_PIPELINE_CE("CE2"), .REG_PIPELINE_RST("RST2"),
			.RESETMODE("SYNC")
		) mult0 (
			.CLK0(CLK), .CE0(CEA0), .RST0(RSTA0), .CE1(CEB0), .RST1(RSTB0),
			.CE2(CEP0), .RST2(RSTP0),
			.A0(A0[0]), .A1(A0[1]), .A2(A0[2]), .A3(A0[3]), .A4(A0[4]), .A5(A0[5]), .A6(A0[6]), .A7(A0[7]), .A8(A0[8]), .A9(A0[9]), .A10(A0[10]), .A11(A0[11]), .A12(A0[12]), .A13(A0[13]), .A14(A0[14]), .A15(A0[15]), .A16(A0[16]), .A17(A0[17]),
			.B0(B0[0]), .B1(B0[1]), .B2(B0[2]), .B3(B0[3]), .B4(B0[4]), .B5(B0[5]), .B6(B0[6]), .B7(B0[7]), .B8(B0[8]), .B9(B0[9]), .B10(B0[10]), .B11(B0[11]), .B12(B0[12]), .B13(B0[13]), .B14(B0[14]), .B15(B0[15]), .B16(B0[16]), .B17(B0[17]),
			.C0(1'b0), .C1(1'b0), .C2(1'b0), .C3(1'b0), .C4(1'b0), .C5(1'b0), .C6(1'b0), .C7(1'b0), .C8(1'b0), .C9(1'b0), .C10(1'b0), .C11(1'b0), .C12(1'b0), .C13(1'b0), .C14(1'b0), .C15(1'b0), .C16(1'b0), .C17(1'b0),
			.SIGNEDA(SIGNED0 ? 1'b1 : 1'b0), .SIGNEDB(SIGNED0 ? 1'b1 : 1'b0), .SOURCEA(1'b0), .SOURCEB(1'b0),
			.P0(p0[0]), .P1(p0[1]), .P2(p0[2]), .P3(p0[3]), .P4(p0[4]), .P5(p0[5]), .P6(p0[6]), .P7(p0[7]), .P8(p0[8]), .P9(p0[9]), .P10(p0[10]), .P11(p0[11]), .P12(p0[12]), .P13(p0[13]), .P14(p0[14]), .P15(p0[15]), .P16(p0[16]), .P17(p0[17]), .P18(p0[18]), .P19(p0[19]), .P20(p0[20]), .P21(p0[21]), .P22(p0[22]), .P23(p0[23]), .P24(p0[24]), .P25(p0[25]), .P26(p0[26]), .P27(p0[27]), .P28(p0[28]), .P29(p0[29]), .P30(p0[30]), .P31(p0[31]), .P32(p0[32]), .P33(p0[33]), .P34(p0[34]), .P35(p0[35]),
			.ROA0(ra0[0]), .ROA1(ra0[1]), .ROA2(ra0[2]), .ROA3(ra0[3]), .ROA4(ra0[4]), .ROA5(ra0[5]), .ROA6(ra0[6]), .ROA7(ra0[7]), .ROA8(ra0[8]), .ROA9(ra0[9]), .ROA10(ra0[10]), .ROA11(ra0[11]), .ROA12(ra0[12]), .ROA13(ra0[13]), .ROA14(ra0[14]), .ROA15(ra0[15]), .ROA16(ra0[16]), .ROA17(ra0[17]),
			.ROB0(rb0[0]), .ROB1(rb0[1]), .ROB2(rb0[2]), .ROB3(rb0[3]), .ROB4(rb0[4]), .ROB5(rb0[5]), .ROB6(rb0[6]), .ROB7(rb0[7]), .ROB8(rb0[8]), .ROB9(rb0[9]), .ROB10(rb0[10]), .ROB11(rb0[11]), .ROB12(rb0[12]), .ROB13(rb0[13]), .ROB14(rb0[14]), .ROB15(rb0[15]), .ROB16(rb0[16]), .ROB17(rb0[17]),
			.SIGNEDP(s0)
		);
	end else begin
		MULT18X18D mult0 (
			.A0(A0[0]), .A1(A0[1]), .A2(A0[2]), .A3(A0[3]), .A4(A0[4]), .A5(A0[5]), .A6(A0[6]), .A7(A0[7]), .A8(A0[8]), .A9(A0[9]), .A10(A0[10]), .A11(A0[11]), .A12(A0[12]), .A13(A0[13]), .A14(A0[14]), .A15(A0[15]), .A16(A0[16]), .A17(A0[17]),
			.B0(B0[0]), .B1(B0[1]), .B2(B0[2]), .B3(B0[3]), .B4(B0[4]), .B5(B0[5]), .B6(B0[6]), .B7(B0[7]), .B8(B0[8]), .B9(B0[9]), .B10(B0[10]), .B11(B0[11]), .B12(B0[12]), .B13(B0[13]), .B14(B0[14]), .B15(B0[15]), .B16(B0[16]), .B17(B0[17]),
			.C0(1'b0), .C1(1'b0), .C2(1'b0), .C3(1'b0), .C4(1'b0), .C5(1'b0), .C6(1'b0), .C7(1'b0), .C8(1'b0), .C9(1'b0), .C10(1'b0), .C11(1'b0), .C12(1'b0), .C13(1'b0), .C14(1'b0), .C15(1'b0), .C16(1'b0), .C17(1'b0),
			.SIGNEDA(SIGNED0 ? 1'b1 : 1'b0), .SIGNEDB(SIGNED0 ? 1'b1 : 1'b0), .SOURCEA(1'b0), .SOURCEB(1'b0),
			.P0(p0[0]), .P1(p0[1]), .P2(p0[2]), .P3(p0[3]), .P4(p0[4]), .P5(p0[5]), .P6(p0[6]), .P7(p0[7]), .P8(p0[8]), .P9(p0[9]), .P10(p0[10]), .P11(p0[11]), .P12(p0[12]), .P13(p0[13]), .P14(p0[14]), .P15(p0[15]), .P16(p0[16]), .P17(p0[17]), .P18(p0[18]), .P19(p0[19]), .P20(p0[20]), .P21(p0[21]), .P22(p0[22]), .P23(p0[23]), .P24(p0[24]), .P25(p0[25]), .P26(p0[26]), .P27(p0[27]), .P28(p0[28]), .P29(p0[29]), .P30(p0[30]), .P31(p0[31]), .P32(p0[32]), .P33(p0[33]), .P34(p0[34]), .P35(p0[35]),
			.ROA0(ra0[0]), .ROA1(ra0[1]), .ROA2(ra0[2]), .ROA3(ra0[3]), .ROA4(ra0[4]), .ROA5(ra0[5]), .ROA6(ra0[6]), .ROA7(ra0[7]), .ROA8(ra0[8]), .ROA9(ra0[9]), .ROA10(ra0[10]), .ROA11(ra0[11]), .ROA12(ra0[12]), .ROA13(ra0[13]), .ROA14(ra0[14]), .ROA15(ra0[15]), .ROA16(ra0[16]), .ROA17(ra0[17]),
			.ROB0(rb0[0]), .ROB1(rb0[1]), .ROB2(rb0[2]), .ROB3(rb0[3]), .ROB4(rb0[4]), .ROB5(rb0[5]), .ROB6(rb0[6]), .ROB7(rb0[7]), .ROB8(rb0[8]), .ROB9(rb0[9]), .ROB10(rb0[10]), .ROB11(rb0[11]), .ROB12(rb0[12]), .ROB13(rb0[13]), .ROB14(rb0[14]), .ROB15(rb0[15]), .ROB16(rb0[16]), .ROB17(rb0[17]),
			.SIGNEDP(s0)
		);
	end endgenerate
	generate if (REGIN1 || PIPE1) begin
		MULT18X18D #(
			.REG_INPUTA_CLK(REGIN1 ? "CLK0" : "NONE"), .REG_INPUTA_CE("CE0"), .REG_INPUTA_RST("RST0"),
			.REG_INPUTB_CLK(REGIN1 ? "CLK0" : "NONE"), .REG_INPUTB_CE("CE1"), .REG_INPUTB_RST("RST1"),
			.REG_PIPELINE_CLK(PIPE1 ? "CLK0" : "NONE"), .REG_PIPELINE_CE("CE2"), .REG_PIPELINE_RST("RST2"),
			.RESETMODE("SYNC")
		) mult1 (
			.CLK0(CLK), .CE0(CEA1), .RST0(RSTA1), .CE1(CEB1), .RST1(RSTB1),
			.CE2(CEP1), .RST2(RSTP1),
			.A0(A1[0]), .A1(A1[1]), .A2(A1[2]), .A3(A1[3]), .A4(A1[4]), .A5(A1[5]), .A6(A1[6]), .A7(A1[7]), .A8(A1[8]), .A9(A1[9]), .A10(A1[10]), .A11(A1[11]), .A12(A1[12]), .A13(A1[13]), .A14(A1[14]), .A15(A1[15]), .A16(A1[16]), .A17(A1[17]),
			.B0(B1[0]), .B1(B1[1]), .B2(B1[2]), .B3(B1[3]), .B4(B1[4]), .B5(B1[5]), .B6(B1[6]), .B7(B1[7]), .B8(B1[8]), .B9(B1[9]), .B10(B1[10]), .B11(B1[11]), .B12(B1[12]), .B13(B1[13]), .B14(B1[14]), .B15(B1[15]), .B16(B1[16]), .B17(B1[17]),
			.C0(1'b0), .C1(1'b0), .C2(1'b0), .C3(1'b0), .C4(1'b0), .C5(1'b0), .C6(1'b0), .C7(1'b0), .C8(1'b0), .C9(1'b0), .C10(1'b0), .C11(1'b0), .C12(1'b0), .C13(1'b0), .C14(1'b0), .C15(1'b0), .C16(1'b0), .C17(1'b0),
			.SIGNEDA(SIGNED1 ? 1'b1 : 1'b0), .SIGNEDB(SIGNED1 ? 1'b1 : 1'b0), .SOURCEA(1'b0), .SOURCEB(1'b0),
			.P0(p1[0]), .P1(p1[1]), .P2(p1[2]), .P3(p1[3]), .P4(p1[4]), .P5(p1[5]), .P6(p1[6]), .P7(p1[7]), .P8(p1[8]), .P9(p1[9]), .P10(p1[10]), .P11(p1[11]), .P12(p1[12]), .P13(p1[13]), .P14(p1[14]), .P15(p1[15]), .P16(p1[16]), .P17(p1[17]), .P18(p1[18]), .P19(p1[19]), .P20(p1[20]), .P21(p1[21]), .P22(p1[22]), .P23(p1[23]), .P24(p1[24]), .P25(p1[25]), .P26(p1[26]), .P27(p1[27]), .P28(p1[28]), .P29(p1[29]), .P30(p1[30]), .P31(p1[31]), .P32(p1[32]), .P33(p1[33]), .P34(p1[34]), .P35(p1[35]),
			.ROA0(ra1[0]), .ROA1(ra1[1]), .ROA2(ra1[2]), .ROA3(ra1[3]), .ROA4(ra1[4]), .ROA5(ra1[5]), .ROA6(ra1[6]), .ROA7(ra1[7]), .ROA8(ra1[8]), .ROA9(ra1[9]), .ROA10(ra1[10]), .ROA11(ra1[11]), .ROA12(ra1[12]), .ROA13(ra1[13]), .ROA14(ra1[14]), .ROA15(ra1[15]), .ROA16(ra1[16]), .ROA17(ra1[17]),
			.ROB0(rb1[0]), .ROB1(rb1[1]), .ROB2(rb1[2]), .ROB3(rb1[3]), .ROB4(rb1[4]), .ROB5(rb1[5]), .ROB6(rb1[6]), .ROB7(rb1[7]), .ROB8(rb1[8]), .ROB9(rb1[9]), .ROB10(rb1[10]), .ROB11(rb1[11]), .ROB12(rb1[12]), .ROB13(rb1[13]), .ROB14(rb1[14]), .ROB15(rb1[15]), .ROB16(rb1[16]), .ROB17(rb1[17]),
			.SIGNEDP(s1)
		);
	end else begin
		MULT18X18D mult1 (
			.A0(A1[0]), .A1(A1[1]), .A2(A1[2]), .A3(A1[3]), .A4(A1[4]), .A5(A1[5]), .A6(A1[6]), .A7(A1[7]), .A8(A1[8]), .A9(A1[9]), .A10(A1[10]), .A11(A1[11]), .A12(A1[12]), .A13(A1[13]), .A14(A1[14]), .A15(A1[15]), .A16(A1[16]), .A17(A1[17]),
			.B0(B1[0]), .B1(B1[1]), .B2(B1[2]), .B3(B1[3]), .B4(B1[4]), .B5(B1[5]), .B6(B1[6]), .B7(B1[7]), .B8(B1[8]), .B9(B1[9]), .B10(B1[10]), .B11(B1[11]), .B12(B1[12]), .B13(B1[13]), .B14(B1[14]), .B15(B1[15]), .B16(B1[16]), .B17(B1[17]),
			.C0(1'b0), .C1(1'b0), .C2(1'b0), .C3(1'b0), .C4(1'b0), .C5(1'b0), .C6(1'b0), .C7(1'b0), .C8(1'b0), .C9(1'b0), .C10(1'b0), .C11(1'b0), .C12(1'b0), .C13(1'b0), .C14(1'b0), .C15(1'b0), .C16(1'b0), .C17(1'b0),
			.SIGNEDA(SIGNED1 ? 1'b1 : 1'b0), .SIGNEDB(SIGNED1 ? 1'b1 : 1'b0), .SOURCEA(1'b0), .SOURCEB(1'b0),
			.P0(p1[0]), .P1(p1[1]), .P2(p1[2]), .P3(p1[3]), .P4(p1[4]), .P5(p1[5]), .P6(p1[6]), .P7(p1[7]), .P8(p1[8]), .P9(p1[9]), .P10(p1[10]), .P11(p1[11]), .P12(p1[12]), .P13(p1[13]), .P14(p1[14]), .P15(p1[15]), .P16(p1[16]), .P17(p1[17]), .P18(p1[18]), .P19(p1[19]), .P20(p1[20]), .P21(p1[21]), .P22(p1[22]), .P23(p1[23]), .P24(p1[24]), .P25(p1[25]), .P26(p1[26]), .P27(p1[27]), .P28(p1[28]), .P29(p1[29]), .P30(p1[30]), .P31(p1[31]), .P32(p1[32]), .P33(p1[33]), .P34(p1[34]), .P35(p1[35]),
			.ROA0(ra1[0]), .ROA1(ra1[1]), .ROA2(ra1[2]), .ROA3(ra1[3]), .ROA4(ra1[4]), .ROA5(ra1[5]), .ROA6(ra1[6]), .ROA7(ra1[7]), .ROA8(ra1[8]), .ROA9(ra1[9]), .ROA10(ra1[10]), .ROA11(ra1[11]), .ROA12(ra1[12]), .ROA13(ra1[13]), .ROA14(ra1[14]), .ROA15(ra1[15]), .ROA16(ra1[16]), .ROA17(ra1[17]),
			.ROB0(rb1[0]), .ROB1(rb1[1]), .ROB2(rb1[2]), .ROB3(rb1[3]), .ROB4(rb1[4]), .ROB5(rb1[5]), .ROB6(rb1[6]), .ROB7(rb1[7]), .ROB8(rb1[8]), .ROB9(rb1[9]), .ROB10(rb1[10]), .ROB11(rb1[11]), .ROB12(rb1[12]), .ROB13(rb1[13]), .ROB14(rb1[14]), .ROB15(rb1[15]), .ROB16(rb1[16]), .ROB17(rb1[17]),
			.SIGNEDP(s1)
		);
	end endgenerate
	generate if (CSEL == 2) begin
		ALU54B #(
			.REG_OUTPUT0_CLK("CLK0"), .REG_OUTPUT0_CE("CE0"), .REG_OUTPUT0_RST("RST0"),
			.REG_OUTPUT1_CLK("CLK0"), .REG_OUTPUT1_CE("CE0"), .REG_OUTPUT1_RST("RST0"),
			.RNDPAT(RNDPAT), .RESETMODE("SYNC")
		) _TECHMAP_REPLACE_ (
			.A0(ra0[0]), .A1(ra0[1]), .A2(ra0[2]), .A3(ra0[3]), .A4(ra0[4]), .A5(ra0[5]), .A6(ra0[6]), .A7(ra0[7]), .A8(ra0[8]), .A9(ra0[9]), .A10(ra0[10]), .A11(ra0[11]), .A12(ra0[12]), .A13(ra0[13]), .A14(ra0[14]), .A15(ra0[15]), .A16(ra0[16]), .A17(ra0[17]),
			.A18(rb0[0]), .A19(rb0[1]), .A20(rb0[2]), .A21(rb0[3]), .A22(rb0[4]), .A23(rb0[5]), .A24(rb0[6]), .A25(rb0[7]), .A26(rb0[8]), .A27(rb0[9]), .A28(rb0[10]), .A29(rb0[11]), .A30(rb0[12]), .A31(rb0[13]), .A32(rb0[14]), .A33(rb0[15]), .A34(rb0[16]), .A35(rb0[17]),
			.B0(ra1[0]), .B1(ra1[1]), .B2(ra1[2]), .B3(ra1[3]), .B4(ra1[4]), .B5(ra1[5]), .B6(ra1[6]), .B7(ra1[7]), .B8(ra1[8]), .B9(ra1[9]), .B10(ra1[10]), .B11(ra1[11]), .B12(ra1[12]), .B13(ra1[13]), .B14(ra1[14]), .B15(ra1[15]), .B16(ra1[16]), .B17(ra1[17]),
			.B18(rb1[0]), .B19(rb1[1]), .B20(rb1[2]), .B21(rb1[3]), .B22(rb1[4]), .B23(rb1[5]), .B24(rb1[6]), .B25(rb1[7]), .B26(rb1[8]), .B27(rb1[9]), .B28(rb1[10]), .B29(rb1[11]), .B30(rb1[12]), .B31(rb1[13]), .B32(rb1[14]), .B33(rb1[15]), .B34(rb1[16]), .B35(rb1[17]),
			.MA0(p0[0]), .MA1(p0[1]), .MA2(p0[2]), .MA3(p0[3]), .MA4(p0[4]), .MA5(p0[5]), .MA6(p0[6]), .MA7(p0[7]), .MA8(p0[8]), .MA9(p0[9]), .MA10(p0[10]), .MA11(p0[11]), .MA12(p0[12]), .MA13(p0[13]), .MA14(p0[14]), .MA15(p0[15]), .MA16(p0[16]), .MA17(p0[17]), .MA18(p0[18]), .MA19(p0[19]), .MA20(p0[20]), .MA21(p0[21]), .MA22(p0[22]), .MA23(p0[23]), .MA24(p0[24]), .MA25(p0[25]), .MA26(p0[26]), .MA27(p0[27]), .MA28(p0[28]), .MA29(p0[29]), .MA30(p0[30]), .MA31(p0[31]), .MA32(p0[32]), .MA33(p0[33]), .MA34(p0[34]), .MA35(p0[35]),
			.MB0(p1[0]), .MB1(p1[1]), .MB2(p1[2]), .MB3(p1[3]), .MB4(p1[4]), .MB5(p1[5]), .MB6(p1[6]), .MB7(p1[7]), .MB8(p1[8]), .MB9(p1[9]), .MB10(p1[10]), .MB11(p1[11]), .MB12(p1[12]), .MB13(p1[13]), .MB14(p1[14]), .MB15(p1[15]), .MB16(p1[16]), .MB17(p1[17]), .MB18(p1[18]), .MB19(p1[19]), .MB20(p1[20]), .MB21(p1[21]), .MB22(p1[22]), .MB23(p1[23]), .MB24(p1[24]), .MB25(p1[25]), .MB26(p1[26]), .MB27(p1[27]), .MB28(p1[28]), .MB29(p1[29]), .MB30(p1[30]), .MB31(p1[31]), .MB32(p1[32]), .MB33(p1[33]), .MB34(p1[34]), .MB35(p1[35]),
			.C0(C[0]), .C1(C[1]), .C2(C[2]), .C3(C[3]), .C4(C[4]), .C5(C[5]), .C6(C[6]), .C7(C[7]), .C8(C[8]), .C9(C[9]), .C10(C[10]), .C11(C[11]), .C12(C[12]), .C13(C[13]), .C14(C[14]), .C15(C[15]), .C16(C[16]), .C17(C[17]), .C18(C[18]), .C19(C[19]), .C20(C[20]), .C21(C[21]), .C22(C[22]), .C23(C[23]), .C24(C[24]), .C25(C[25]), .C26(C[26]), .C27(C[27]), .C28(C[28]), .C29(C[29]), .C30(C[30]), .C31(C[31]), .C32(C[32]), .C33(C[33]), .C34(C[34]), .C35(C[35]), .C36(C[36]), .C37(C[37]), .C38(C[38]), .C39(C[39]), .C40(C[40]), .C41(C[41]), .C42(C[42]), .C43(C[43]), .C44(C[44]), .C45(C[45]), .C46(C[46]), .C47(C[47]), .C48(C[48]), .C49(C[49]), .C50(C[50]), .C51(C[51]), .C52(C[52]), .C53(C[53]),
			.OP0(op[0]), .OP1(op[1]), .OP2(op[2]), .OP3(op[3]), .OP4(op[4]), .OP5(op[5]), .OP6(op[6]), .OP7(op[7]), .OP8(op[8]), .OP9(op[9]), .OP10(op[10]),
			.SIGNEDIA(s0), .SIGNEDIB(s1),
			.CLK0(CLK), .CE0(CE), .RST0(RST),
			.R0(r[0]), .R1(r[1]), .R2(r[2]), .R3(r[3]), .R4(r[4]), .R5(r[5]), .R6(r[6]), .R7(r[7]), .R8(r[8]), .R9(r[9]), .R10(r[10]), .R11(r[11]), .R12(r[12]), .R13(r[13]), .R14(r[14]), .R15(r[15]), .R16(r[16]), .R17(r[17]), .R18(r[18]), .R19(r[19]), .R20(r[20]), .R21(r[21]), .R22(r[22]), .R23(r[23]), .R24(r[24]), .R25(r[25]), .R26(r[26]), .R27(r[27]), .R28(r[28]), .R29(r[29]), .R30(r[30]), .R31(r[31]), .R32(r[32]), .R33(r[33]), .R34(r[34]), .R35(r[35]), .R36(r[36]), .R37(r[37]), .R38(r[38]), .R39(r[39]), .R40(r[40]), .R41(r[41]), .R42(r[42]), .R43(r[43]), .R44(r[44]), .R45(r[45]), .R46(r[46]), .R47(r[47]), .R48(r[48]), .R49(r[49]), .R50(r[50]), .R51(r[51]), .R52(r[52]), .R53(r[53])
		);
	end else begin
		ALU54B #(
			.REG_OUTPUT0_CLK("CLK0"), .REG_OUTPUT0_CE("CE0"), .REG_OUTPUT0_RST("RST0"),
			.REG_OUTPUT1_CLK("CLK0"), .REG_OUTPUT1_CE("CE0"), .REG_OUTPUT1_RST("RST0"),
			.RNDPAT(RNDPAT), .RESETMODE("SYNC")
		) _TECHMAP_REPLACE_ (
			.A0(ra0[0]), .A1(ra0[1]), .A2(ra0[2]), .A3(ra0[3]), .A4(ra0[4]), .A5(ra0[5]), .A6(ra0[6]), .A7(ra0[7]), .A8(ra0[8]), .A9(ra0[9]), .A10(ra0[10]), .A11(ra0[11]), .A12(ra0[12]), .A13(ra0[13]), .A14(ra0[14]), .A15(ra0[15]), .A16(ra0[16]), .A17(ra0[17]),
			.A18(rb0[0]), .A19(rb0[1]), .A20(rb0[2]), .A21(rb0[3]), .A22(rb0[4]), .A23(rb0[5]), .A24(rb0[6]), .A25(rb0[7]), .A26(rb0[8]), .A27(rb0[9]), .A28(rb0[10]), .A29(rb0[11]), .A30(rb0[12]), .A31(rb0[13]), .A32(rb0[14]), .A33(rb0[15]), .A34(rb0[16]), .A35(rb0[17]),
			.B0(ra1[0]), .B1(ra1[1]), .B2(ra1[2]), .B3(ra1[3]), .B4(ra1[4]), .B5(ra1[5]), .B6(ra1[6]), .B7(ra1[7]), .B8(ra1[8]), .B9(ra1[9]), .B10(ra1[10]), .B11(ra1[11]), .B12(ra1[12]), .B13(ra1[13]), .B14(ra1[14]), .B15(ra1[15]), .B16(ra1[16]), .B17(ra1[17]),
			.B18(rb1[0]), .B19(rb1[1]), .B20(rb1[2]), .B21(rb1[3]), .B22(rb1[4]), .B23(rb1[5]), .B24(rb1[6]), .B25(rb1[7]), .B26(rb1[8]), .B27(rb1[9]), .B28(rb1[10]), .B29(rb1[11]), .B30(rb1[12]), .B31(rb1[13]), .B32(rb1[14]), .B33(rb1[15]), .B34(rb1[16]), .B35(rb1[17]),
			.MA0(p0[0]), .MA1(p0[1]), .MA2(p0[2]), .MA3(p0[3]), .MA4(p0[4]), .MA5(p0[5]), .MA6(p0[6]), .MA7(p0[7]), .MA8(p0[8]), .MA9(p0[9]), .MA10(p0[10]), .MA11(p0[11]), .MA12(p0[12]), .MA13(p0[13]), .MA14(p0[14]), .MA15(p0[15]), .MA16(p0[16]), .MA17(p0[17]), .MA18(p0[18]), .MA19(p0[19]), .MA20(p0[20]), .MA21(p0[21]), .MA22(p0[22]), .MA23(p0[23]), .MA24(p0[24]), .MA25(p0[25]), .MA26(p0[26]), .MA27(p0[27]), .MA28(p0[28]), .MA29(p0[29]), .MA30(p0[30]), .MA31(p0[31]), .MA32(p0[32]), .MA33(p0[33]), .MA34(p0[34]), .MA35(p0[35]),
			.MB0(p1[0]), .MB1(p1[1]), .MB2(p1[2]), .MB3(p1[3]), .MB4(p1[4]), .MB5(p1[5]), .MB6(p1[6]), .MB7(p1[7]), .MB8(p1[8]), .MB9(p1[9]), .MB10(p1[10]), .MB11(p1[11]), .MB12(p1[12]), .MB13(p1[13]), .MB14(p1[14]), .MB15(p1[15]), .MB16(p1[16]), .MB17(p1[17]), .MB18(p1[18]), .MB19(p1[19]), .MB20(p1[20]), .MB21(p1[21]), .MB22(p1[22]), .MB23(p1[23]), .MB24(p1[24]), .MB25(p1[25]), .MB26(p1[26]), .MB27(p1[27]), .MB28(p1[28]), .MB29(p1[29]), .MB30(p1[30]), .MB31(p1[31]), .MB32(p1[32]), .MB33(p1[33]), .MB34(p1[34]), .MB35(p1[35]),
			.OP0(op[0]), .OP1(op[1]), .OP2(op[2]), .OP3(op[3]), .OP4(op[4]), .OP5(op[5]), .OP6(op[6]), .OP7(op[7]), .OP8(op[8]), .OP9(op[9]), .OP10(op[10]),
			.SIGNEDIA(s0), .SIGNEDIB(s1),
			.CLK0(CLK), .CE0(CE), .RST0(RST),
			.R0(r[0]), .R1(r[1]), .R2(r[2]), .R3(r[3]), .R4(r[4]), .R5(r[5]), .R6(r[6]), .R7(r[7]), .R8(r[8]), .R9(r[9]), .R10(r[10]), .R11(r[11]), .R12(r[12]), .R13(r[13]), .R14(r[14]), .R15(r[15]), .R16(r[16]), .R17(r[17]), .R18(r[18]), .R19(r[19]), .R20(r[20]), .R21(r[21]), .R22(r[22]), .R23(r[23]), .R24(r[24]), .R25(r[25]), .R26(r[26]), .R27(r[27]), .R28(r[28]), .R29(r[29]), .R30(r[30]), .R31(r[31]), .R32(r[32]), .R33(r[33]), .R34(r[34]), .R35(r[35]), .R36(r[36]), .R37(r[37]), .R38(r[38]), .R39(r[39]), .R40(r[40]), .R41(r[41]), .R42(r[42]), .R43(r[43]), .R44(r[44]), .R45(r[45]), .R46(r[46]), .R47(r[47]), .R48(r[48]), .R49(r[49]), .R50(r[50]), .R51(r[51]), .R52(r[52]), .R53(r[53])
		);
	end endgenerate
endmodule
