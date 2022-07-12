#define r_type_insn(_f7, _rs2, _rs1, _f3, _rd, _opc) \
.word (((_f7) << 25) | ((_rs2) << 20) | ((_rs1) << 15) | ((_f3) << 12) | ((_rd) << 7) | ((_opc) << 0))

#define xgsoc_retirq_insn() \
r_type_insn(0b0000010, 0, 0, 0b000, 0, 0b0001011)
