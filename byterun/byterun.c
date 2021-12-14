/* Lama SM Bytecode interpreter */

# include <string.h>
# include <stdio.h>
# include <errno.h>
# include <malloc.h>
# include "../runtime/runtime.h"

void *__start_custom_data;
void *__stop_custom_data;

enum cmd1 {
  BINOP = 0,
  LD = 2,
  LDA = 3,
  ST = 4,
  PATT = 6,
  CALL_EXTERNAL = 7,
  END_OF_FILE = 15
};

enum cmd2 {
  CONST,
  STRING,
  SEXP,
  STI,
  STA,
  JMP,
  END,
  RET,
  DROP,
  DUP,
  SWAP,
  ELEM
};

enum cmd3 {
  CJMPZ,
  CJMPNZ,
  BEGIN,
  CBEGIN,
  CLOSURE,
  CALLC,
  CALL,
  TAG,
  ARRAY,
  FAIL,
  LINE
};

enum binop {
  BINOP_PLUS,
  BINOP_MINUS,
  BINOP_MUL,
  BINOP_DIV,
  BINOP_MOD,
  BINOP_LT,
  BINOP_LE,
  BINOP_GT,
  BINOP_GE,
  BINOP_EQ,
  BINOP_NEQ,
  BINOP_AND,
  BINOP_OR
};

enum MEM {
  MEM_GLOB,
  MEM_LOC,
  MEM_ARG,
  MEM_ACC
};

enum builtin {
  LREAD,
  LWRITE,
  LLENGTH,
  LSTRING,
  BARRAY
};

enum patt { 
  PATT_STR,
  PATT_STRING,
  PATT_ARRAY,
  PATT_SEXP,
  PATT_REF,
  PATT_VAL,
  PATT_FUN
};

const int STACK_SIZE = 256 * 1024 * 1024;

/* The unpacked representation of bytecode file */
typedef struct {
  char *string_ptr;              /* A pointer to the beginning of the string table */
  int  *public_ptr;              /* A pointer to the beginning of publics table    */
  char *code_ptr;                /* A pointer to the bytecode itself               */
  int  *global_ptr;              /* A pointer to the global area                   */
  int   stringtab_size;          /* The size (in bytes) of the string table        */
  int   global_area_size;        /* The size (in words) of global area             */
  int   public_symbols_number;   /* The number of public symbols                   */
  char  buffer[0];               
} bytefile;

/* Gets a string from a string table by an index */
char* get_string (bytefile *f, int pos) {
  return &f->string_ptr[pos];
}

/* Gets a name for a public symbol */
char* get_public_name (bytefile *f, int i) {
  return get_string (f, f->public_ptr[i*2]);
}

/* Gets an offset for a publie symbol */
int get_public_offset (bytefile *f, int i) {
  return f->public_ptr[i*2+1];
}

/* Reads a binary bytecode file by name and unpacks it */
bytefile* read_file (char *fname) {
  FILE *f = fopen (fname, "rb");
  long size;
  bytefile *file;

  if (f == 0) {
    failure ("%s\n", strerror (errno));
  }
  
  if (fseek (f, 0, SEEK_END) == -1) {
    failure ("%s\n", strerror (errno));
  }

  file = (bytefile*) malloc (sizeof(int)*4 + (size = ftell (f)));

  if (file == 0) {
    failure ("*** FAILURE: unable to allocate memory.\n");
  }
  
  rewind (f);

  if (size != fread (&file->stringtab_size, 1, size, f)) {
    failure ("%s\n", strerror (errno));
  }
  
  fclose (f);
  
  file->string_ptr  = &file->buffer [file->public_symbols_number * 2 * sizeof(int)];
  file->public_ptr  = (int*) file->buffer;
  file->code_ptr    = &file->string_ptr [file->stringtab_size];
  file->global_ptr  = (int*) malloc (file->global_area_size * sizeof (int));
  
  return file;
}

/* Disassembles the bytecode pool */
void disassemble (FILE *f, bytefile *bf) {
  
# define INT    (ip += sizeof (int), *(int*)(ip - sizeof (int)))
# define BYTE   *ip++
# define FAILURE  failure ("ERROR: invalid opcode %d-%d\n", h, l)
  
  char *ip     = bf->code_ptr;
  char *ops [] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
  char *pats[] = {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"};
  char *lds [] = {"LD", "LDA", "ST"};
  do {
    char x = BYTE,
         h = (x & 0xF0) >> 4,
         l = x & 0x0F;

    fprintf (f, "0x%.8x:\t", ip-bf->code_ptr-1);
    
    switch (h) {
    case 15:
      goto stop;
      
    /* BINOP */
    case 0:
      fprintf (f, "BINOP\t%s", ops[l-1]);
      break;
      
    case 1:
      switch (l) {
      case  0:
        fprintf (f, "CONST\t%d", INT);
        break;
        
      case  1:
        fprintf (f, "STRING\t%s", get_string (bf, INT));
        break;
          
      case  2:
        fprintf (f, "SEXP\t%s ", get_string (bf, INT));
        fprintf (f, "%d", INT);
        break;
        
      case  3:
        fprintf (f, "STI");
        break;
        
      case  4:
        fprintf (f, "STA");
        break;
        
      case  5:
        fprintf (f, "JMP\t0x%.8x", INT);
        break;
        
      case  6:
        fprintf (f, "END");
        break;
        
      case  7:
        fprintf (f, "RET");
        break;
        
      case  8:
        fprintf (f, "DROP");
        break;
        
      case  9:
        fprintf (f, "DUP");
        break;
        
      case 10:
        fprintf (f, "SWAP");
        break;

      case 11:
        fprintf (f, "ELEM");
        break;
        
      default:
        FAILURE;
      }
      break;
      
    case 2:
    case 3:
    case 4:
      fprintf (f, "%s\t", lds[h-2]);
      switch (l) {
      case 0: fprintf (f, "G(%d)", INT); break;
      case 1: fprintf (f, "L(%d)", INT); break;
      case 2: fprintf (f, "A(%d)", INT); break;
      case 3: fprintf (f, "C(%d)", INT); break;
      default: FAILURE;
      }
      break;
      
    case 5:
      switch (l) {
      case  0:
        fprintf (f, "CJMPz\t0x%.8x", INT);
        break;
        
      case  1:
        fprintf (f, "CJMPnz\t0x%.8x", INT);
        break;
        
      case  2:
        fprintf (f, "BEGIN\t%d ", INT);
        fprintf (f, "%d", INT);
        break;
        
      case  3:
        fprintf (f, "CBEGIN\t%d ", INT);
        fprintf (f, "%d", INT);
        break;
        
      case  4:
        fprintf (f, "CLOSURE\t0x%.8x", INT);
        {int n = INT;
         for (int i = 0; i<n; i++) {
         switch (BYTE) {
           case 0: fprintf (f, "G(%d)", INT); break;
           case 1: fprintf (f, "L(%d)", INT); break;
           case 2: fprintf (f, "A(%d)", INT); break;
           case 3: fprintf (f, "C(%d)", INT); break;
           default: FAILURE;
         }
         }
        };
        break;
          
      case  5:
        fprintf (f, "CALLC\t%d", INT);
        break;
        
      case  6:
        fprintf (f, "CALL\t0x%.8x ", INT);
        fprintf (f, "%d", INT);
        break;
        
      case  7:
        fprintf (f, "TAG\t%s ", get_string (bf, INT));
        fprintf (f, "%d", INT);
        break;
        
      case  8:
        fprintf (f, "ARRAY\t%d", INT);
        break;
        
      case  9:
        fprintf (f, "FAILURE\t%d ", INT);
        fprintf (f, "%d", INT);
        break;
        
      case 10:
        fprintf (f, "LINE\t%d", INT);
        break;

      default:
        FAILURE;
      }
      break;
      
    case 6:
      fprintf (f, "PATT\t%s", pats[l]);
      break;

    case 7: {
      switch (l) {
      case 0:
        fprintf (f, "CALL\tLread");
        break;
        
      case 1:
        fprintf (f, "CALL\tLwrite");
        break;

      case 2:
        fprintf (f, "CALL\tLlength");
        break;

      case 3:
        fprintf (f, "CALL\tLstring");
        break;

      case 4:
        fprintf (f, "CALL\tBarray\t%d", INT);
        break;

      default:
        FAILURE;
      }
    }
    break;
      
    default:
      FAILURE;
    }

    fprintf (f, "\n");
  }
  while (1);
 stop: fprintf (f, "<end>\n");
}

/* Dumps the contents of the file */
void dump_file (FILE *f, bytefile *bf) {
  int i;
  
  fprintf (f, "String table size       : %d\n", bf->stringtab_size);
  fprintf (f, "Global area size        : %d\n", bf->global_area_size);
  fprintf (f, "Number of public symbols: %d\n", bf->public_symbols_number);
  fprintf (f, "Public symbols          :\n");

  for (i=0; i < bf->public_symbols_number; i++) 
    fprintf (f, "   0x%.8x: %s\n", get_public_offset (bf, i), get_public_name (bf, i));

  fprintf (f, "Code:\n");
  disassemble (f, bf);
}

typedef struct activation {
  int* args;
  int* locals;
  int* accesses;
  struct activation* old_fp;
  char* old_ip;
} activation;

void interpreter(bytefile *bf, char* filename) {
  const char* ip = bf->code_ptr;
  int* sp_base = malloc(STACK_SIZE * sizeof(int));
  int* sp = sp_base;
  activation* fp = (activation*) sp;
  __asm__("movl %0, __gc_stack_bottom" : : "r" (sp_base));
  __init();
  __asm__("movl %0, __gc_stack_top" : : "r" (sp));
  void check_sp(int new_bytes) {
    if ((void*) sp - (void*) sp_base + new_bytes > STACK_SIZE * sizeof(int)) {
      failure("stack overflow\n");
    }
  }
  void push(int x) {
    check_sp(sizeof(int));
    *sp = x;
    sp++;
    __asm__("movl %0, __gc_stack_top" : : "r" (sp));
  }
  int pop() {
    sp--;
    __asm__("movl %0, __gc_stack_top" : : "r" (sp));
    return *sp;
  }
  int peek() {
    return *(sp - 1);
  }
  do {
    char x = BYTE,
         h = (x & 0xF0) >> 4,
         l = x & 0x0F;
    switch (h) {
      case END_OF_FILE:
        free(sp_base);
        return;
        
      case BINOP:
        ; int y = UNBOX(pop());
        int x = UNBOX(pop());

        switch (l - 1) {
          case BINOP_PLUS: push(BOX(x + y)); break;
          case BINOP_MINUS: push(BOX(x - y)); break;
          case BINOP_MUL: push(BOX(x * y)); break;
          case BINOP_DIV: push(BOX(x / y)); break;
          case BINOP_MOD: push(BOX(x % y)); break;
          case BINOP_LT: push(BOX(x < y)); break;
          case BINOP_LE: push(BOX(x <= y)); break;
          case BINOP_GT: push(BOX(x > y)); break;
          case BINOP_GE: push(BOX(x >= y)); break;
          case BINOP_EQ: push(BOX(x == y)); break;
          case BINOP_NEQ: push(BOX(x != y)); break;
          case BINOP_AND: push(BOX(x && y)); break;
          case BINOP_OR: push(BOX(x || y)); break;
          default: FAILURE;
        }
        break;
        
      case 1:
        switch (l) {
        case  CONST:
          push(BOX(INT));
          break;
          
        case STRING:
          push(Bstring(bf->string_ptr + INT));
          break;
            
        case SEXP:
          __asm__("pushl %0" : : "r" (LtagHash(bf->string_ptr + INT)));
          int ar = INT;
          for (int i = 0; i < ar; i++) {
            __asm__("pushl %0" : : "r" (pop()));
          }
          __asm__("pushl %0" : : "r" (BOX(ar + 1)));
          __asm__("call Bsexp");
          __asm__("movl %eax, %ebx");
          __asm__("addl %0, %%esp" : : "r" (sizeof(int) * (ar + 2)));
          register int ebx asm("ebx");
          push(ebx);
          break;
          
        case STA:
          ; int v = pop();
          int i2 = pop();
          int x2 = pop();
          push(Bsta(v, i2, x2));
          break;
          
        case JMP:
          ip = bf->code_ptr + INT;
          break;
          
        case END:
        case RET:
          if (fp->old_fp == sp_base) {
            //returning from main
            free(sp_base);
            return;
          }
          int res = pop();
          ip = fp->old_ip;
          sp = fp->args;
          __asm__("movl %0, __gc_stack_top" : : "r" (sp));
          fp = fp->old_fp;
          push(res);
          break;
          
        case DROP:
          pop();
          break;
          
        case DUP:
          push(peek());
          break;
          
        case SWAP:
          ; int x = pop();
          int y = pop();
          push(x);
          push(y);
          break;

        case ELEM:
          ; int i = pop();
          int p = pop();
          push(Belem(p, i));
          break;
          
        default:
          FAILURE;
        }
        break;
        
      case LD:
        switch (l) {
        case MEM_GLOB: push(*(bf->global_ptr + INT)); break;
        case MEM_LOC: push(*(fp->locals + INT)); break;
        case MEM_ARG: push(*(fp->args + INT)); break;
        case MEM_ACC: push(* (int*) *(fp->accesses + INT)); break;
        default: FAILURE;
        }
        break;
      case LDA:
        switch (l) {
        case MEM_GLOB: ; int ptr = bf->global_ptr + INT; push(ptr); push(ptr); break;
        case MEM_LOC: ptr = fp->locals + INT; push(ptr); push(ptr); break;
        case MEM_ARG: ptr = fp->args + INT; push(ptr); push(ptr); break;
        case MEM_ACC: ptr = *(fp->accesses + INT); push(ptr); push(ptr); break;
        default: FAILURE;
        }
        break;
      case ST:
        switch (l) {
        case MEM_GLOB: *(bf->global_ptr + INT) = peek(); break;
        case MEM_LOC: *(fp->locals + INT) = peek(); break;
        case MEM_ARG: *(fp->args + INT) = peek(); break;
        case MEM_ACC: *(int*)*(fp->accesses + INT) = peek(); break;
        default: FAILURE;
        }
        break;
        
      case 5:
        switch (l) {
        case CJMPZ:
          ; int ofs = INT;
          if (!UNBOX(pop())) {
            ip = bf->code_ptr + ofs;
          }
          break;
          
        case CJMPNZ:
          ; ofs = INT;
          if (UNBOX(pop())) {
            ip = bf->code_ptr + ofs;
          }
          break;
          
        case BEGIN:
          ; activation act;
          act.args = sp;
          sp += INT + 1;
          act.old_ip = pop();
          act.accesses = sp;
          act.old_fp = fp;
          act.locals = sp;
          sp += INT;
          fp = sp;
          check_sp(sizeof(activation));
          memcpy(sp, &act, sizeof(activation));
          sp = (int*) ((char*) sp + sizeof(activation));
          __asm__("movl %0, __gc_stack_top" : : "r" (sp));
          break;
          
        case CBEGIN:
          act.args = sp;
          sp += INT + 2;
          int accN = pop();
          act.old_ip = pop();
          sp += 2;
          act.accesses = sp;
          sp += accN;
          act.old_fp = fp;
          act.locals = sp;
          sp += INT;
          fp = sp;
          check_sp(sizeof(activation));
          memcpy(sp, &act, sizeof(activation));
          sp = (int*) ((char*) sp + sizeof(activation));
          __asm__("movl %0, __gc_stack_top" : : "r" (sp));
          break;
          
        case CLOSURE:
          ; int addr = INT;
          int n = INT;
          for (int i = 0; i < n; i++) {
            switch (BYTE) {
              case MEM_GLOB: __asm__("pushl %0" : : "r" (*(bf->global_ptr + INT))); break;
              case MEM_LOC: __asm__("pushl %0" : : "r" (*(fp->locals + INT))); break;
              case MEM_ARG: __asm__("pushl %0" : : "r" (*(fp->args + INT))); break;
              case MEM_ACC: __asm__("pushl %0" : : "r" (* (int*) *(fp->accesses + INT))); break;
              default: FAILURE;
            }
          }
          __asm__("pushl %0" : : "r" (addr));
          __asm__("pushl %0" : : "r" (BOX(n)));
          __asm__("call Bclosure");
          __asm__("movl %eax, %ebx");
          __asm__("addl %0, %%esp" : : "r" (sizeof(int) * (n + 1)));
          register int ebx asm("ebx");
          push(ebx);
        break;
            
        case CALLC:
          ; int argN = INT;
          data* r = TO_DATA(*(sp - argN - 1));
          memmove(sp - argN - 1, sp - argN, argN * sizeof(int));
          sp--;
          ofs = *(int*)r->contents;
          accN = LEN(r->tag) - 1;
          push(ip);
          push(accN);
          for (int i = accN - 1; i >= 0; i--) {
            push(((int*)r->contents) + i + 1);
          }
          ip = bf->code_ptr + ofs;
          sp -= argN + accN + 2;
          break;
          
        case CALL:
          ofs = INT;
          argN = INT;
          push(ip);
          push(0);
          ip = bf->code_ptr + ofs;
          sp -= argN + 2;
          break;
          
        case TAG:
          ; int h = LtagHash(bf->string_ptr + INT);
          push(Btag(pop(), h, BOX(INT)));
          break;
          
        case ARRAY:
          push(Barray_patt(pop(), BOX(INT)));
          break;
          
        case FAIL:
          ; int a = BOX(INT);
          int b = BOX(INT);
          Bmatch_failure(pop(), filename, a, b);
          break;
          
        case LINE:
          INT;
          break;

        default:
          FAILURE;
        }
        break;
        
      case PATT:
        switch (l) {
        case PATT_STR: push(Bstring_patt(pop(), pop())); break;
        case PATT_STRING: push(Bstring_tag_patt(pop())); break;
        case PATT_ARRAY: push(Barray_tag_patt(pop())); break;
        case PATT_SEXP: push(Bsexp_tag_patt(pop())); break;
        case PATT_REF: push(Bboxed_patt(pop())); break;
        case PATT_VAL: push(Bunboxed_patt(pop())); break;
        case PATT_FUN: push(Bclosure_tag_patt(pop())); break;
        default: FAILURE;
        }
        break;

      case CALL_EXTERNAL: {
        switch (l) {
        case LREAD: push(Lread()); break;
        case LWRITE: push(Lwrite(pop())); break;
        case LLENGTH: push(Llength(pop())); break;
        case LSTRING: push(Lstring(pop())); break;
        case BARRAY:
          ; int n = INT;
          for (int i = 0; i < n; i++) {
            __asm__("pushl %0" : : "r" (pop()));
          }
          __asm__("pushl %0" : : "r" (BOX(n)));
          __asm__("call Barray");
          __asm__("movl %eax, %ebx");
          __asm__("addl %0, %%esp" : : "r" (sizeof(int) * (n + 1)));
          register int ebx asm("ebx");
          push(ebx);
          break;
        default:
          FAILURE;
        }
      }
      break;
        
      default:
        FAILURE;
    }
  }
  while (1);
}

int main (int argc, char* argv[]) {
  bytefile *f = read_file (argv[1]);
  //dump_file (stdout, f);
  interpreter (f, argv[1]);
  free(f->global_ptr);
  free(f);
  return 0;
}
