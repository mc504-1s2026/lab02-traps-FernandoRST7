#include <arch/timer.h>
#include <kernel/panic.h>
#include <arch/csr.h>
#include <kernel/serial.h>

#define SIE_STIE (1UL << 5)

static int alarm_active = 0;

u64 timer_read()
{
	return csr_read(CSR_TIME);
}

void timer_irq_enable()
{
	u64 sie = csr_read(CSR_SIE);
	csr_write(CSR_SIE, sie | SIE_STIE);
}

void timer_irq_disable()
{
	u64 sie = csr_read(CSR_SIE);
	csr_write(CSR_SIE, sie & ~SIE_STIE);
}

void timer_set_alarm(u64 secs)
{
	u64 now = timer_read();
	u64 target = now + (secs * TIMER_FREQ);
	csr_write(CSR_STIMECMP, target);
	alarm_active = 1;
}

void timer_irq()
{
	if (alarm_active) {
		serial_puts("alarm\n");
		alarm_active = 0;
	}
	/* Desativa a interrupção do timer jogando-a muito pro futuro, 
	 * até o próximo alarm ser setado */
	csr_write(CSR_STIMECMP, 0xFFFFFFFFFFFFFFFFULL);
}