#include <kernel/printf.h>
#include <kernel/mm.h>
#include <arch/timer.h>
#include <kernel/trap.h>
#include <kernel/serial.h>

/* Helpers estáticos para não depender de bibliotecas externas */
static int my_strncmp(const char *s1, const char *s2, size_t n) {
	while (n && *s1 && (*s1 == *s2)) {
		++s1;
		++s2;
		--n;
	}
	if (n == 0) return 0;
	return (*(unsigned char *)s1 - *(unsigned char *)s2);
}

static u64 my_atoi(const char *str) {
	u64 res = 0;
	while (*str >= '0' && *str <= '9') {
		res = res * 10 + (u64)(*str - '0');
		str++;
	}
	return res;
}

/* Helper simples para imprimir um u64 seguido da letra 's' e quebra de linha na serial */
static void print_uptime(u64 secs) {
	char buf[32];
	int i = 0;
	
	if (secs == 0) {
		serial_puts("0s\n");
		return;
	}
	
	while (secs > 0 && i < 30) {
		buf[i++] = (char)('0' + (secs % 10));
		secs /= 10;
	}
	
	/* Imprime os dígitos na ordem correta (invertida) */
	while (i > 0) {
		serial_putc(buf[--i]);
	}
	serial_puts("s\n");
}

extern int _hartid[];
void kmain()
{
	printk_set_level(LOG_DEBUG);
	info("entered S-mode\n");
	info("booting on hart %d\n", _hartid[0]);
	info("setting up virtual memory...\n");
	vm_init();

	info("enabling traps...\n");
	trap_setup();
	info("enabling timer...\n");
	timer_irq_enable();
	info("enabling serial...\n");
	serial_init();
	serial_irq_enable();

	/* Habilitação global de interrupções! Muito importante para o shell funcionar. */
	hart_irq_enable(); 

	/* Variáveis do nosso pequeno Shell */
	char cmd_buffer[256];
	size_t cmd_idx = 0;
	char read_buf[256];

	serial_puts("> ");

	while (1) {
		/* Lê o que chegou assincronamente da interrupção */
		size_t n = serial_read(read_buf);
		
		for (size_t i = 0; i < n; i++) {
			char c = read_buf[i];

			/* Carriage Return (0x0d) ou Newline */
			if (c == '\r' || c == '\n') {
				serial_puts("\n");
				cmd_buffer[cmd_idx] = '\0';

				if (cmd_idx > 0) {
					/* Parse command: uptime */
					if (my_strncmp(cmd_buffer, "uptime", 6) == 0 && (cmd_buffer[6] == '\0' || cmd_buffer[6] == ' ')) {
						u64 now = timer_read();
						u64 secs = now / TIMER_FREQ;
						print_uptime(secs);
					} 
					/* Parse command: echo [str] */
					else if (my_strncmp(cmd_buffer, "echo ", 5) == 0) {
						serial_puts(cmd_buffer + 5);
						serial_puts("\n");
					} 
					/* Parse command: alarm [time] */
					else if (my_strncmp(cmd_buffer, "alarm ", 6) == 0) {
						u64 secs = my_atoi(cmd_buffer + 6);
						timer_set_alarm(secs);
					} 
					else {
						serial_puts("Unknown command: ");
						serial_puts(cmd_buffer);
						serial_puts("\n");
					}
				}

				cmd_idx = 0;
				serial_puts("> ");
			} 
			/* Backspace simples */
			else if (c == 127 || c == '\b') {
				if (cmd_idx > 0) {
					cmd_idx--;
					serial_puts("\b \b");
				}
			} 
			/* Adiciona caractere e faz o "echo" (imprimir a tela) */
			else {
				if (cmd_idx < sizeof(cmd_buffer) - 1) {
					cmd_buffer[cmd_idx++] = c;
					serial_putc(c);
				}
			}
		}
	}
}