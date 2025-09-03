#include <state.h>
#include "z80_decode.h"


char cc_holds(char cond);



struct tab_elt
{
  unsigned char val;
  unsigned char mask;
  void * (*fp)(void *pc, struct tab_elt *inst);
  unsigned char inst_len;
} ;

/* PSEUDO EVAL FUNCTIONS */
static void *rst         (void *pc, const struct tab_elt *inst);
static void *pref_cb     (void *pc, const struct tab_elt *inst);
static void *pref_ed     (void *pc, const struct tab_elt *inst);
static void *pref_ind    (void *pc, const struct tab_elt *inst);
static void *pref_xd_cb  (void *pc, const struct tab_elt *inst);
static void *pe_djnz     (void *pc, const struct tab_elt *inst);
static void *pe_jp_nn    (void *pc, const struct tab_elt *inst);
static void *pe_jp_cc_nn (void *pc, const struct tab_elt *inst);
static void *pe_jp_hl    (void *pc, const struct tab_elt *inst);
static void *pe_jr       (void *pc, const struct tab_elt *inst);
static void *pe_jr_cc    (void *pc, const struct tab_elt *inst);
static void *pe_ret      (void *pc, const struct tab_elt *inst);
static void *pe_ret_cc   (void *pc, const struct tab_elt *inst);
static void *pe_rst      (void *pc, const struct tab_elt *inst);
static void *pe_dummy    (void *pc, const struct tab_elt *inst);
/* end of pseudo eval functions */

/* Table to disassemble machine codes without prefix.  */
const struct tab_elt opc_main[] =
{
  { 0x00, 0xFF, pe_dummy    ,  1 }, // "nop",           
  { 0x01, 0xCF, pe_dummy    ,  3 }, // "ld %s,0x%%04x", 
  { 0x02, 0xFF, pe_dummy    ,  1 }, // "ld (bc),a",     
  { 0x03, 0xCF, pe_dummy    ,  1 }, // "inc " ,         
  { 0x04, 0xC7, pe_dummy    ,  1 }, // "inc %s",        
  { 0x05, 0xC7, pe_dummy    ,  1 }, // "dec %s",        
  { 0x06, 0xC7, pe_dummy    ,  2 }, // "ld %s,0x%%02x", 
  { 0x07, 0xFF, pe_dummy    ,  1 }, // "rlca",          
  { 0x08, 0xFF, pe_dummy    ,  1 }, // "ex af,af'",     
  { 0x09, 0xCF, pe_dummy    ,  1 }, // "add hl,",       
  { 0x0A, 0xFF, pe_dummy    ,  1 }, // "ld a,(bc)" ,    
  { 0x0B, 0xCF, pe_dummy    ,  1 }, // "dec ",          
  { 0x0F, 0xFF, pe_dummy    ,  1 }, // "rrca",          
  { 0x10, 0xFF, pe_djnz     ,  2 }, // "djnz ",         
  { 0x12, 0xFF, pe_dummy    ,  1 }, // "ld (de),a",     
  { 0x17, 0xFF, pe_dummy    ,  1 }, // "rla",           
  { 0x18, 0xFF, pe_jr       ,  2 }, // "jr ",           
  { 0x1A, 0xFF, pe_dummy    ,  1 }, // "ld a,(de)",     
  { 0x1F, 0xFF, pe_dummy    ,  1 }, // "rra",           
  { 0x20, 0xE7, pe_jr_cc    ,  2 }, // "jr %s,",        
  { 0x22, 0xFF, pe_dummy    ,  3 }, // "ld (0x%04x),hl",
  { 0x27, 0xFF, pe_dummy    ,  1 }, // "daa",           
  { 0x2A, 0xFF, pe_dummy    ,  3 }, // "ld hl,(0x%04x)",
  { 0x2F, 0xFF, pe_dummy    ,  1 }, // "cpl",           
  { 0x32, 0xFF, pe_dummy    ,  3 }, // "ld (0x%04x),a", 
  { 0x37, 0xFF, pe_dummy    ,  1 }, // "scf",           
  { 0x3A, 0xFF, pe_dummy    ,  3 }, // "ld a,(0x%04x)", 
  { 0x3F, 0xFF, pe_dummy    ,  1 }, // "ccf",           
                                    //                  
  { 0x76, 0xFF, pe_dummy    ,  1 }, // "halt",          
  { 0x40, 0xC0, pe_dummy    ,  1 }, // "ld %s,%s",      
                                    //                  
  { 0x80, 0xC0, pe_dummy    ,  1 }, // "%s%s",          
                                    //                  
  { 0xC0, 0xC7, pe_ret_cc   ,  1 }, // "ret ",          
  { 0xC1, 0xCF, pe_dummy    ,  1 }, // "pop",           
  { 0xC2, 0xC7, pe_jp_cc_nn ,  3 }, // "jp ",           
  { 0xC3, 0xFF, pe_jp_nn    ,  3 }, // "jp 0x%04x",     
  { 0xC4, 0xC7, pe_jp_cc_nn ,  3 }, // "call ",         
  { 0xC5, 0xCF, pe_dummy    ,  1 }, // "push",          
  { 0xC6, 0xC7, pe_dummy    ,  2 }, // "%s0x%%02x",     
  { 0xC7, 0xC7, pe_rst      ,  1 }, // "rst 0x%02x",    
  { 0xC9, 0xFF, pe_ret      ,  1 }, // "ret",           
  { 0xCB, 0xFF, pref_cb     ,  2 }, // "",              
  { 0xCD, 0xFF, pe_jp_nn    ,  3 }, // "call 0x%04x",   
  { 0xD3, 0xFF, pe_dummy    ,  2 }, // "out (0x%02x),a",
  { 0xD9, 0xFF, pe_dummy    ,  1 }, // "exx",           
  { 0xDB, 0xFF, pe_dummy    ,  2 }, // "in a,(0x%02x)", 
  { 0xDD, 0xFF, pref_ind    ,  0 }, // "ix",            
  { 0xE3, 0xFF, pe_dummy    ,  1 }, // "ex (sp),hl",    
  { 0xE9, 0xFF, pe_jp_hl    ,  1 }, // "jp (hl)",
  { 0xEB, 0xFF, pe_dummy    ,  1 }, // "ex de,hl",      
  { 0xED, 0xFF, pref_ed     ,  0 }, // "",              
  { 0xF3, 0xFF, pe_dummy    ,  1 }, // "di",            
  { 0xF9, 0xFF, pe_dummy    ,  1 }, // "ld sp,hl",      
  { 0xFB, 0xFF, pe_dummy    ,  1 }, // "ei",            
  { 0xFD, 0xFF, pref_ind    ,  0 }, // "iy",            
  { 0x00, 0x00, pe_dummy    ,  1 }, // "????"
} ;

/* ED prefix opcodes table.
   Note the instruction length does include the ED prefix (+ 1 byte)
*/
const struct tab_elt opc_ed[] =
{
  { 0x70, 0xFF, pe_dummy, 1 + 1 }, // "in f,(c)"       
  { 0x70, 0xFF, pe_dummy, 1 + 1 }, // "xx"             
  { 0x40, 0xC7, pe_dummy, 1 + 1 }, // "in %s,(c)"      
  { 0x71, 0xFF, pe_dummy, 1 + 1 }, // "out (c),0"      
  { 0x70, 0xFF, pe_dummy, 1 + 1 }, // "xx"             
  { 0x41, 0xC7, pe_dummy, 1 + 1 }, // "out (c),%s"     
  { 0x42, 0xCF, pe_dummy, 1 + 1 }, // "sbc hl,"        
  { 0x43, 0xCF, pe_dummy, 1 + 3 }, // "ld (0x%%04x),%s"
  { 0x44, 0xFF, pe_dummy, 1 + 1 }, // "neg"            
  { 0x45, 0xFF, pe_ret  , 1 + 1 }, // "retn"           
  { 0x46, 0xFF, pe_dummy, 1 + 1 }, // "im 0"           
  { 0x47, 0xFF, pe_dummy, 1 + 1 }, // "ld i,a"         
  { 0x4A, 0xCF, pe_dummy, 1 + 1 }, // "adc hl,"        
  { 0x4B, 0xCF, pe_dummy, 1 + 3 }, // "ld %s,(0x%%04x)"
  { 0x4D, 0xFF, pe_ret  , 1 + 1 }, // "reti"           
  { 0x4F, 0xFF, pe_dummy, 1 + 1 }, // "ld r,a"         
  { 0x56, 0xFF, pe_dummy, 1 + 1 }, // "im 1"           
  { 0x57, 0xFF, pe_dummy, 1 + 1 }, // "ld a,i"         
  { 0x5E, 0xFF, pe_dummy, 1 + 1 }, // "im 2"           
  { 0x5F, 0xFF, pe_dummy, 1 + 1 }, // "ld a,r"         
  { 0x67, 0xFF, pe_dummy, 1 + 1 }, // "rrd"            
  { 0x6F, 0xFF, pe_dummy, 1 + 1 }, // "rld"            
  { 0xA0, 0xE4, pe_dummy, 1 + 1 }, // ""               
  { 0xC3, 0xFF, pe_dummy, 1 + 1 }, // "muluw hl,bc"    
  { 0xC5, 0xE7, pe_dummy, 1 + 1 }, // "mulub a,%s"     
  { 0xF3, 0xFF, pe_dummy, 1 + 1 }, // "muluw hl,sp"    
  { 0x00, 0x00, pe_dummy, 1 + 1 }  // "xx"             
};

/* table for FD and DD prefixed instructions */
const struct tab_elt opc_ind[] =
{
  { 0x24, 0xF7, pe_dummy   , 1 }, // "inc %s%%s"            
  { 0x25, 0xF7, pe_dummy   , 1 }, // "dec %s%%s"            
  { 0x26, 0xF7, pe_dummy   , 2 }, // "ld %s%%s,0x%%%%02x"   
  { 0x21, 0xFF, pe_dummy   , 3 }, // "ld %s,0x%%04x"        
  { 0x22, 0xFF, pe_dummy   , 3 }, // "ld (0x%%04x),%s"      
  { 0x2A, 0xFF, pe_dummy   , 3 }, // "ld %s,(0x%%04x)"      
  { 0x23, 0xFF, pe_dummy   , 1 }, // "inc %s"               
  { 0x2B, 0xFF, pe_dummy   , 1 }, // "dec %s"               
  { 0x29, 0xFF, pe_dummy   , 1 }, // "%s"                   
  { 0x09, 0xCF, pe_dummy   , 1 }, // "add %s,"              
  { 0x34, 0xFF, pe_dummy   , 2 }, // "inc (%s%%+d)"         
  { 0x35, 0xFF, pe_dummy   , 2 }, // "dec (%s%%+d)"         
  { 0x36, 0xFF, pe_dummy   , 3 }, // "ld (%s%%+d),0x%%%%02x"
                        
  { 0x76, 0xFF, pe_dummy   , 1 }, // "h"                    
  { 0x46, 0xC7, pe_dummy   , 2 }, // "ld %%s,(%s%%%%+d)"    
  { 0x70, 0xF8, pe_dummy   , 2 }, // "ld (%s%%%%+d),%%s"    
  { 0x64, 0xF6, pe_dummy   , 1 }, // "%s"                   
  { 0x60, 0xF0, pe_dummy   , 1 }, // "ld %s%%s,%%s"         
  { 0x44, 0xC6, pe_dummy   , 1 }, // "ld %%s,%s%%s"         
                        
  { 0x86, 0xC7, pe_dummy   , 2 }, // "%%s(%s%%%%+d)"        
  { 0x84, 0xC6, pe_dummy   , 1 }, // "%%s%s%%s"             
                            
  { 0xE1, 0xFF, pe_dummy   , 1 }, // "pop %s"               
  { 0xE5, 0xFF, pe_dummy   , 1 }, // "push %s"              
  { 0xCB, 0xFF, pref_xd_cb , 3 }, // "%s"                   
  { 0xE3, 0xFF, pe_dummy   , 1 }, // "ex (sp),%s"           
  { 0xE9, 0xFF, pe_dummy   , 1 }, // "jp (%s)"              
  { 0xF9, 0xFF, pe_dummy   , 1 }, // "ld sp,%s"             
  { 0x00, 0x00, pe_dummy   , 1 }, // "?"                    
};

//Calculate location to place step breakpoint
uint16_t calculateStep(void)
{
  char *instrMem;
  char *nextInstrMem;
  unsigned short opcode;

  struct tab_elt *p;

  instrMem = (char *) gdbserver_state.registers[REGISTERS_PC];

  opcode = *instrMem;
//  stepped = 1;

  for (p = opc_main; p->val != (opcode & p->mask); ++p)
    ;

  nextInstrMem = (char *) p->fp(instrMem, p);

  instrMem = nextInstrMem;
//  instrBuffer.memAddr = instrMem;
//  instrBuffer.oldInstr = *instrMem;
  return (uint16_t) instrMem;
}

static void *
pe_dummy (void *pc, const struct tab_elt *inst)
{
  char *cpc = (char *)pc;
  return (cpc + inst->inst_len);
}

void *
pe_rst (void *pc, const struct tab_elt *inst)
{
  char *cpc = (char *)pc;
  char opcode = *cpc;
  
  char rst_mask            = ~(inst->mask);
  unsigned char rst        = (opcode & rst_mask);
  unsigned char target_rst = (rst >> 3) & 0x07;
  
  return (target_rst * 8);
}

void *
pref_cb (void *pc, const struct tab_elt *inst)
{
  char *cpc = (char *)pc;
  // all CB prefixed instructions have the same length (2 bytes)
  return (cpc + inst->inst_len);
}

void *
pref_ind (void *pc, const struct tab_elt *inst)
{
  struct tab_elt *p;
  char *cpc = (char *)pc;

  for (p = opc_ind; p->val != (cpc[1] & p->mask); ++p)
    ;
  return (cpc + 
          1   + // FD or DD  prefix
          p->inst_len);
}

void *
pref_xd_cb (void *pc, const struct tab_elt *inst)
{
  char *cpc = (char *)pc;
  return cpc + inst->inst_len;
}

static void *
pref_ed (void *pc, const struct tab_elt *inst)
{
  struct tab_elt *p;
  char *cpc = (char *)pc;

  for (p = opc_ed; p->val != (cpc[1] & p->mask); ++p)
    ;
  return p->fp(cpc, p);
}
// -------------------- 

//---------- CONTROL JUMP INSTRUCTIONS PSEUDO EVAL FUNCTIONS ----------
void *
pe_djnz (void *pc, const struct tab_elt *inst)
{
  char *cpc = (char *)pc;
  short b = ((unsigned short)gdbserver_state.registers[REGISTERS_BC] >> 8);

  if (b - 1 == 0)
    return (cpc + inst->inst_len); 
  else
    {    // result of dec wasn't Z, so we jump e
      short e = cpc[1];
      return (cpc + e + 2);
    }
}

void *
pe_jp_nn (void *pc, const struct tab_elt *inst)
{
  char *cpc = (char *)pc;
  short nn  = *(short *)(cpc+1); // immediate nn for the jp
  return (nn);
}

void *
pe_jp_cc_nn (void *pc, const struct tab_elt *inst)
{
  char *cpc = (char *)pc;
  short e = 0;

  char opcode = *cpc;
  char condition_mask = ~(inst->mask);
  unsigned char condition = (opcode & condition_mask);
  condition = (condition >> 3) & 0x07;

  if (cc_holds(condition))
    {
      // jump is effective
      // pc will jump to the immediate address
      return pe_jp_nn(pc, inst);
    }
  else
    { // conditional jump won't take place
      // move PC to the next instruction
      return (cpc + inst->inst_len);
    }
}

void *
pe_jp_hl (void *pc, const struct tab_elt *inst)
{
  char *jp_addr  = (char *) gdbserver_state.registers[REGISTERS_HL];
  return (jp_addr);
}

void *
pe_jr (void *pc, const struct tab_elt *inst)
{
  char *cpc = (char *)pc;
  int e =  cpc[1]; //  relative offset for the jump
  return (cpc + e + 2);
}

// condition codes values (000, 001, 010, ... 111)
enum { 
  cond_NZ, 
  cond_Z, 
  cond_NC,
  cond_C,
  cond_PO,
  cond_PE,
  cond_P,
  cond_M
};


char cc_holds(char cond)
{
// Z80 FLAG BITFIELD: SZ5H3PNC
#define SIGN_FLAG_MASK     0x80 // (1 << 7) 
#define ZERO_FLAG_MASK     0x40 // (1 << 6) 
#define PARITY_FLAG_MASK   0x04 // (1 << 2) 
#define CARRY_FLAG_MASK    0x01 // (1 << 0)  

  char flags = gdbserver_state.registers[REGISTERS_AF] & 0xFF ; // get the flags byte
  char holds = 0;
  switch (cond)
    {
    case cond_NZ:
      holds = !(flags & ZERO_FLAG_MASK);
      break;
    case cond_Z:
      holds =  (flags & ZERO_FLAG_MASK);
      break;
    case cond_NC:
      holds = !(flags & CARRY_FLAG_MASK);
      break;
    case cond_C:
      holds =  (flags & CARRY_FLAG_MASK);
      break;
    case cond_PO:
      holds = !(flags & PARITY_FLAG_MASK);
      break;
    case cond_PE:
      holds =  (flags & PARITY_FLAG_MASK);
      break;
    case cond_P:
      holds = !(flags & SIGN_FLAG_MASK);
      break;
    case cond_M:
      holds =  (flags & SIGN_FLAG_MASK);
      break;
    }
  return holds;
}

void *
pe_jr_cc (void *pc, const struct tab_elt *inst)
{
  char *cpc = (char *)pc;
  short e = 0;

  char opcode = *cpc;
  char condition_mask = ~(inst->mask);
  unsigned char condition = (opcode & condition_mask);
  condition = (condition >> 3) & 0x07;

  if (cc_holds(condition))
    {
      // jump is effective
      e =  cpc[1]; //  relative offset for the jump
    }

  return (cpc + e + 2);
}

void *
pe_ret (void *pc, const struct tab_elt *inst)
{
  void *ret_addr = (void *) *((short *)gdbserver_state.registers[REGISTERS_SP]); // get the return address from the TOS
  return ret_addr;
}

void *
pe_ret_cc (void *pc, const struct tab_elt *inst)
{
  char *cpc = (char *)pc;
  char opcode = *cpc;
  char condition_mask = ~(inst->mask);
  unsigned char condition = (opcode & condition_mask);
  condition = (condition >> 3) & 0x07;
  
  if (cc_holds(condition))
    {
      // ret is effective
      return pe_ret(pc, inst);
    }
  else
    { // conditional ret won't take place
      // move PC to the next instruction
      return (cpc + inst->inst_len);
    }
}
