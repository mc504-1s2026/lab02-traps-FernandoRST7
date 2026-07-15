#include <kernel/trap.h>
#include <kernel/panic.h>
#include <arch/csr.h>
#include <kernel/printf.h>
#include <arch/plic.h>
#include <arch/timer.h>
#include <kernel/serial.h>

#define SSTATUS_SIE (1UL << 1)

/* defined in src/trap_entry.S */
extern void trap_entry();

void handle_irq()
{
	u64 cause = csr_read(CSR_SCAUSE);
	
	if (cause == TRAP_TIMER_IRQ) {
		timer_irq();
	} else if (cause == TRAP_EXTERNAL_IRQ) {
		/* Hart 0 faz o claim do IRQ do PLIC */
		u32 irq = plic_hart_claim_irq(0);
		
		if (irq == IRQ_SERIAL) {
			serial_irq();
		}
		
		/* Avisa o PLIC que concluímos a interrupção */
		if (irq != 0) {
			plic_hart_complete_irq(0, irq);
		}
	} else {
		warn("Unknown IRQ cause: %llu\n", cause);
	}
}

void handle_exception()
{
	u64 cause = csr_read(CSR_SCAUSE);
	u64 epc = csr_read(CSR_SEPC);
	u64 tval = csr_read(CSR_STVAL);
	
	error("Exception %llu at sepc=0x%llx, stval=0x%llx\n", cause, epc, tval);
	panic("Unhandled exception!");
}

void trap_setup()
{
	/* Configura o endereço base de traps e desabilita interrupções globais no momento */
	csr_write(CSR_STVEC, (u64)trap_entry);
	hart_irq_disable();
}

void handle_trap()
{
	u64 cause = csr_read(CSR_SCAUSE);
	
	if (cause & TRAP_IRQ_BIT) {
		handle_irq();
	} else {
		handle_exception();
	}
}

void hart_irq_enable()
{
	u64 status = csr_read(CSR_SSTATUS);
	csr_write(CSR_SSTATUS, status | SSTATUS_SIE);
}

u64 hart_irq_save()
{
	u64 status = csr_read(CSR_SSTATUS);
	hart_irq_disable();
	return (status & SSTATUS_SIE);
}

void hart_irq_restore(u64 flags)
{
	if (flags) {
		hart_irq_enable();
	} else {
		hart_irq_disable();
	}
}

void hart_irq_disable()
{
	u64 status = csr_read(CSR_SSTATUS);
	csr_write(CSR_SSTATUS, status & ~SSTATUS_SIE);
}