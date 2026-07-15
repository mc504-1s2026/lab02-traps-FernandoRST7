#include <kernel/serial.h>
#include <kernel/panic.h>
#include <arch/csr.h>
#include <arch/plic.h>
#include <arch/spinlock.h>
#include <kernel/trap.h>

/* Conversão exigida para acessar a memória física via mapeamento direto (Higher Half Kernel) */
#define KERNEL_DIRECT_MAP_START 0xFFFFFFC000000000ULL
#define SERIAL_BASE_VA ((volatile u8 *)(KERNEL_DIRECT_MAP_START + (u64)SERIAL_BASE))

#define SIE_SEIE (1UL << 9)
#define SERIAL_BUF_SIZE 256

struct serialdev {
	char buf[SERIAL_BUF_SIZE];
	size_t len;
	size_t head;
	size_t tail;
	struct spinlock lock;
};

/* Como 'dev' é static, o compilador C garante que todos os campos sejam inicializados com 0 */
static struct serialdev dev = {
	.len = 0,
	.head = 0,
	.tail = 0
};

static inline void serial_write_reg(u32 offset, u8 val) {
	SERIAL_BASE_VA[offset] = val;
}

static inline u8 serial_read_reg(u32 offset) {
	return SERIAL_BASE_VA[offset];
}

void serial_init()
{
	dev.len = 0;
	dev.head = 0;
	dev.tail = 0;

	/* Habilita FIFO, limpa TX e RX buffers */
	serial_write_reg(SERIAL_FCR, SERIAL_FCR_FIFO_ENABLE | SERIAL_FCR_RX_FIFO_CLEAR | SERIAL_FCR_TX_FIFO_CLEAR);
	
	/* Habilita interrupção quando há dado recebido disponível na placa */
	serial_write_reg(SERIAL_IER, SERIAL_IER_ERBFI);

	/* Configuração do PLIC */
	plic_irq_set_priority(IRQ_SERIAL, 1);
	plic_hart_set_threshold(0, 0);
	plic_hart_enable_irq(0, IRQ_SERIAL);
}

void serial_irq_enable()
{
	u64 sie = csr_read(CSR_SIE);
	csr_write(CSR_SIE, sie | SIE_SEIE);
}

void serial_irq_disable()
{
	u64 sie = csr_read(CSR_SIE);
	csr_write(CSR_SIE, sie & ~SIE_SEIE);
}

void serial_irq()
{
	/* Em contexto de interrupção, as interrupções já estão desabilitadas pela CPU no RISC-V */
	spin_lock(&dev.lock);
	
	/* Enquanto a bitflag de 'Dado Pronto' for 1, lê bytes da serial */
	while (serial_read_reg(SERIAL_LSR) & SERIAL_LSR_DTR) {
		u8 c = serial_read_reg(SERIAL_RBR);
		
		if (dev.len < SERIAL_BUF_SIZE) {
			dev.buf[dev.tail] = (char)c;
			dev.tail = (dev.tail + 1) % SERIAL_BUF_SIZE;
			dev.len++;
		}
	}
	
	spin_unlock(&dev.lock);
}

size_t serial_read(char *buf)
{
	/* CRÍTICO: Desabilita interrupções antes de pegar o lock para evitar Deadlock! */
	u64 flags = hart_irq_save();
	spin_lock(&dev.lock);
	
	size_t bytes_read = dev.len;
	for (size_t i = 0; i < bytes_read; i++) {
		buf[i] = dev.buf[dev.head];
		dev.head = (dev.head + 1) % SERIAL_BUF_SIZE;
	}
	dev.len = 0;
	
	spin_unlock(&dev.lock);
	/* Restaura o estado anterior das interrupções */
	hart_irq_restore(flags);
	
	return bytes_read;
}

void serial_putc(char c)
{
	while ((serial_read_reg(SERIAL_LSR) & SERIAL_LSR_THRE) == 0) {
		// Wait
	}
	serial_write_reg(SERIAL_THR, c);
}

void serial_puts(char *str)
{
	while (*str) {
		serial_putc(*str++);
	}
}