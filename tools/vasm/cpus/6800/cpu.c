/*
 * cpu.c 6800 cpu description file
 * (c) in 2013-2016,2019 by Esben Norby and Frank Wille
 */

#include "vasm.h"

mnemonic mnemonics[] = {
#include "opcodes.h"
};

int mnemonic_cnt = sizeof(mnemonics) / sizeof(mnemonics[0]);

int		bitsperbyte = 8;
int		bytespertaddr = 2;
char *		cpu_copyright = "vasm 6800/6801/68hc11 cpu backend 0.4a (c) 2013-2016,2019,2021 Esben Norby";
char *		cpuname = "6800";

static uint8_t	cpu_type = M6800;
static int 	modifier;	/* set by find_base() */


int
init_cpu()
{
	return 1;
}


int
cpu_args(char *p)
{
	if (!strncmp(p, "-m68", 4)) {
		p += 4;
		if (p[0] == '0' && p[3] == '\0') {
			switch(p[1]) {
				case '0':
				case '2':
				case '8':
					/* 6802 and 6808 are a 6800 with embedded ROM/RAM */
					cpu_type = M6800;
					break;
				case '1':
				case '3':
					/* 6803 is a 6801 with embedded ROM/RAM */
					cpu_type = M6801;
					break;
				default:
					/* 6804 and 6805 are not opcode compatible
					 * 6809 is somewhat compatible, but not completely
					 * 6806 and 6807 do not exist, to my knowledge
					 */
					return 0;
			}
		} else if (!stricmp(p, "hc11"))
			cpu_type = M68HC11;
		else
			return 0;
		return 1;
	}
	return 0;
}


char *
parse_cpu_special(char *start)
{
	return start;
}


operand *
new_operand()
{
	operand *new = mymalloc(sizeof(*new));
	new->type = -1;
	return new;
}


int
parse_operand(char *p, int len, operand *op, int required)
{
	char *start = p;

	op->value = NULL;

	switch (required) {
	    case IMM:
	    case IMM16:
		if (*p++ != '#')
			return PO_NOMATCH;
		p = skip(p);
                /* fall through */
	    case REL:
	    case DATAOP:
	    	op->value = parse_expr(&p);
	    	break;

	    case ADDR:
		if (*p == '<') {
			required = DIR;
			p++;
		}
		else if (*p == '>') {
			required = EXT;
			p++;
		}
	    	op->value = parse_expr(&p);
	    	break;

	    case DIR:
		if (*p == '>')
			return PO_NOMATCH;
		else if (*p == '<')
			p++;
	    	op->value = parse_expr(&p);
		break;

	    case EXT:
		if (*p == '<')
			return PO_NOMATCH;
		else if (*p == '>')
			p++;
	    	op->value = parse_expr(&p);
		break;

	    case REGX:
		if (toupper((unsigned char)*p++) != 'X')
			return PO_NOMATCH;
		break;
	    case REGY:
		if (toupper((unsigned char)*p++) != 'Y')
			return PO_NOMATCH;
		break;

	    default:
		return PO_NOMATCH;
	}

	p = skip(p);
	if (p-start < len) {
		cpu_error(0);  /* trailing garbage */
		return PO_CORRUPT;
	}
	op->type = required;
	return PO_MATCH;
}


static size_t
eval_oper(operand *op, section *sec, taddr pc, taddr offs, dblock *db)
{
	size_t size = 0;
        symbol *base = NULL;
        int btype;
	taddr val;

	if (op->value != NULL && !eval_expr(op->value, &val, sec, pc)) {
		modifier = 0;
		btype = find_base(op->value, &base, sec, pc);
	}

	switch (op->type) {
	    case ADDR:
		if (base != NULL || val < 0 || val > 0xff) {
			op->type = EXT;
			size = 2;
		}
		else {
			op->type = DIR;
			size = 1;
		}
		break;
	    case DIR:
		size = 1;
		if (db != NULL && (val < 0 || val > 0xff))
			cpu_error(2);  /* operand doesn't fit into 8-bits */
                break;
	    case IMM:
		size = 1;
		if (db != NULL && !modifier && (val < -0x80 || val > 0xff))
			cpu_error(2);  /* operand doesn't fit into 8-bits */
		break;
	    case EXT:
	    case IMM16:
		size = 2;
		break;
	    case REL:
		size = 1;
		break;
	}

	if (size > 0 && db != NULL) {
		/* create relocation entry and code for this operand */
		if (op->type == REL && base == NULL) {
			/* relative branch to absolute label */
			val = val - (pc + offs + 1);
			if (val < -0x80 || val > 0x7f)
				cpu_error(3); /* branch out of range */
		}
		else if (op->type == REL && base != NULL && btype == BASE_OK) {
			/* relative branches */
			if (!is_pc_reloc(base, sec)) {
				val = val - (pc + offs + 1);
				if (val < -0x80 || val > 0x7f)
					cpu_error(3); /* branch out of range */
			}
			else
				add_extnreloc(&db->relocs, base, val, REL_PC,
				              0, 8, offs);
		}
		else if (base != NULL && btype != BASE_ILLEGAL) {
			rlist *rl;

			rl = add_extnreloc(&db->relocs, base, val,
			                   btype==BASE_PCREL? REL_PC : REL_ABS,
			                   0, size << 3, offs);
			switch (modifier) {
			    case LOBYTE:
				if (rl) ((nreloc *)rl->reloc)->mask = 0xff;
				val &= 0xff;
				break;
			    case HIBYTE:
				if (rl) ((nreloc *)rl->reloc)->mask = 0xff00;
				val = (val >> 8) & 0xff;
				break;
			}
		}
		else if (base != NULL)
			general_error(38);  /* illegal relocation */

		if (size == 1) {
			op->code[0] = val & 0xff;
		}
		else if (size == 2) {
			op->code[0] = (val >> 8) & 0xff;
			op->code[1] = val & 0xff;
		}
		else
			ierror(0);
	}
	return (size);
}


size_t
instruction_size(instruction *ip, section *sec, taddr pc)
{
	operand op;
	int i;
	size_t size;

	size = (mnemonics[ip->code].ext.prebyte != 0) ? 2 : 1;

	for (i = 0; i < MAX_OPERANDS && ip->op[i] != NULL; i++) {
		op = *(ip->op[i]);
		size += eval_oper(&op, sec, pc, size, NULL);
	}

	return (size);
}


dblock *
eval_instruction(instruction *ip, section *sec, taddr pc)
{
	dblock *db = new_dblock();
	uint8_t opcode;
	uint8_t *d;
	int i;
	size_t size;

	/* evaluate operands and determine instruction size */
	opcode = mnemonics[ip->code].ext.opcode;
	size = (mnemonics[ip->code].ext.prebyte != 0) ? 2 : 1;
	for (i = 0; i < MAX_OPERANDS && ip->op[i] != NULL; i++) {
		size += eval_oper(ip->op[i], sec, pc, size, db);
		if (ip->op[i]->type == DIR)
			opcode = mnemonics[ip->code].ext.dir_opcode;
	}

	/* allocate and fill data block */
	db->size = size;
	d = db->data = mymalloc(size);

	if (mnemonics[ip->code].ext.prebyte != 0)
		*d++ = mnemonics[ip->code].ext.prebyte;
	*d++ = opcode;

	/* write operands */
	for (i = 0; i < MAX_OPERANDS && ip->op[i] != NULL; i++) {
		switch (ip->op[i]->type) {
		    case IMM:
		    case DIR:
		    case REL:
			*d++ = ip->op[i]->code[0];
			break;
		    case IMM16:
		    case EXT:
			*d++ = ip->op[i]->code[0];
			*d++ = ip->op[i]->code[1];
			break;
		}
	}

	return (db);
}

dblock *
eval_data(operand *op, size_t bitsize, section *sec, taddr pc)
{
	dblock *db = new_dblock();
	uint8_t *d;
	taddr val;

	if (bitsize != 8 && bitsize != 16)
		cpu_error(1,bitsize);  /* data size not supported */

	db->size = bitsize >> 3;
	d = db->data = mymalloc(db->size);

	if (!eval_expr(op->value, &val, sec, pc)) {
		symbol *base;
		int btype;
		rlist *rl;

		modifier = 0;
		btype = find_base(op->value, &base, sec, pc);
		if (btype==BASE_OK || (btype==BASE_PCREL && modifier==0)) {
			rl = add_extnreloc(&db->relocs, base, val,
			                   btype==BASE_PCREL ? REL_PC : REL_ABS,
			                   0, bitsize, 0);
			switch (modifier) {
			    case LOBYTE:
				if (rl) ((nreloc *)rl->reloc)->mask = 0xff;
				val &= 0xff;
				break;
			    case HIBYTE:
				if (rl) ((nreloc *)rl->reloc)->mask = 0xff00;
				val = (val >> 8) & 0xff;
				break;
			}
		}
		else if (btype != BASE_NONE)
			general_error(38);  /* illegal relocation */
	}

	if (bitsize == 8) {
		if (val < -0x80 || val > 0xff)
			cpu_error(2);  /* operand doesn't fit into 8-bits */
	}
	else  /* 16 bits */
		*d++ = (val >> 8) & 0xff;
	*d = val & 0xff;

	return (db);
}


int
ext_unary_eval(int type, taddr val, taddr *result, int cnst)
{
	switch (type) {
	    case LOBYTE:
		*result = cnst ? (val & 0xff) : val;
		return 1;
	    case HIBYTE:
		*result = cnst ? ((val >> 8) & 0xff) : val;
		return 1;
	    default:
		break;
	}

	return 0;  /* unknown type */
}


int
ext_find_base(symbol **base, expr *p, section *sec, taddr pc)
{
	/* addr/256 equals >addr, addr%256 and addr&255 equal <addr */
	if (p->type==DIV || p->type==MOD) {
		if (p->right->type==NUM && p->right->c.val==256)
			p->type = p->type == DIV ? HIBYTE : LOBYTE;
	}
  	else if (p->type==BAND && p->right->type==NUM && p->right->c.val==255)
		p->type = LOBYTE;

	if (p->type==LOBYTE || p->type==HIBYTE) {
		modifier = p->type;
		return find_base(p->left,base,sec,pc);
	}

	return BASE_ILLEGAL;
}


int
cpu_available(int idx)
{
	return (mnemonics[idx].ext.available & cpu_type) != 0;
}
