/** @file main.c
 *
 */
#include "autoconf.h"
#ifdef CONFIG_ARCH_CORTEX_M4
#include "m4-systick.h"
#else
#error "no systick support for other by now!"
#endif
#include "debug.h"
#include "m4-systick.h"
#include "m4-core.h"
#include "m4-cpu.h"
#include "soc-init.h"
#include "soc-usart.h"
#include "soc-usart-regs.h"
#include "soc-layout.h"
#include "soc-gpio.h"
#include "soc-exti.h"
#include "soc-nvic.h"
#include "soc-rcc.h"
#include "soc-interrupts.h"
#include "boot_mode.h"
#include "stack_check.h"
#include "shared.h"
#include "leds.h"
#include "exported/gpio.h"

/**
 *  Ref DocID022708 Rev 4 p.141
 *  BX/BLX cause usageFault if bit[0] of Rm is O
 *  Rm is a reg that indicates addr to branch to.
 *  Bit[0] of the value in Rm must be 1, BUT the addr
 *  to branch to is created by changing bit[0] to 0
 */
//app_entry_t ldr_main = (app_entry_t)(_ldr_base+1);
app_entry_t fw1_main = (app_entry_t) (FW1_START);
#ifdef CONFIG_FIRMWARE_DUALBANK
app_entry_t fw2_main = (app_entry_t) (FW2_START);
#endif

volatile bool dfu_mode = false;




void exti_button_handler(uint8_t irq __attribute__((unused)),
                         uint32_t sr __attribute__((unused)),
                         uint32_t dr __attribute__((unused)))
{
  dfu_mode = true;
}

extern const shr_vars_t shared_vars;
    volatile uint32_t count = 2;

/*
 * We use the local -fno-stack-protector flag for main because
 * the stack protection has not been initialized yet.
 */
__attribute__ ((optimize("-fno-stack-protector")))
int main(void)
{
    disable_irq();

    /* init MSP for loader */
//    asm volatile ("msr msp, %0\n\t" : : "r" (0x20020000) : "r2");

    system_init((uint32_t) LDR_BASE);
    core_systick_init();
    // button now managed at kernel boot to detect if DFU mode
    //d_button_init();
    debug_console_init();

    enable_irq();

    dbg_log("======= Wookey Loader ========\n");
    dbg_log("Built date\t: %s at %s\n", __DATE__, __TIME__);
#ifdef STM32F429
    dbg_log("Board\t\t: STM32F429\n");
#else
    dbg_log("Board\t\t: STM32F407\n");
#endif
    dbg_log("==============================\n");
    dbg_flush();

#if CONFIG_LOADER_MOCKUP
#if CONFIG_LOADER_MOCKUP_DFU
    dfu_mode = true;
    goto start_boot;
#else
    goto start_boot;
#endif
#endif

    //leds_init();
    //leds_on(PROD_LED_STATUS);
    // button is on GPIO E4
#ifdef CONFIG_FIRMWARE_DFU
    soc_dwt_init();
    soc_exti_init();
    dbg_log("registering button on GPIO E4\n");
    dbg_flush();
    dev_gpio_info_t gpio = {
        .kref.port = GPIO_PE,
        .kref.pin = 4,
        .mask = GPIO_MASK_SET_MODE | GPIO_MASK_SET_PUPD |
            GPIO_MASK_SET_TYPE | GPIO_MASK_SET_SPEED |
            GPIO_MASK_SET_EXTI,
        .mode = GPIO_PIN_INPUT_MODE,
        .pupd = GPIO_PULLDOWN,
        .type = GPIO_PIN_OTYPER_PP,
        .speed = GPIO_PIN_LOW_SPEED,
        .afr = 0,
        .bsr_r = 0,
        .lck = 0,
        .exti_trigger = GPIO_EXTI_TRIGGER_RISE,
        .exti_lock = GPIO_EXTI_LOCKED,
        .exti_handler = (user_handler_t) exti_button_handler
    };
    soc_gpio_set_config(&gpio);
    soc_exti_config(&gpio);
    soc_exti_enable(gpio.kref);
#endif

    // wait 1s for button push...

    dbg_log("Waiting for DFU jump through button push (%d seconds)\n", count);
    uint32_t start, stop;
    do {
        // in millisecs
        start = soc_dwt_getcycles() / 168000;
        do {
        stop = soc_dwt_getcycles() / 168000;
        } while (stop - start < 1000); // < 1s
        dbg_log(".");
        dbg_flush();
        count--;
    } while (count > 0);

    if (count == 0) {
      dbg_log("Booting...\n");
    }

#if CONFIG_LOADER_MOCKUP
start_boot:
#endif

    if (dfu_mode) {
        dbg_log("Jumping to DFU mode: %x\n",
                shared_vars.apps[shared_vars.default_dfu_index].entry_point);
        dbg_log("Geronimo !\n");
        dbg_flush();

        disable_irq();

        shared_vars.apps[shared_vars.default_dfu_index].entry_point();
    } else {
        dbg_log("Jumping to FW mode: %x\n",
                shared_vars.apps[shared_vars.default_app_index].entry_point);
        dbg_log("Geronimo !\n");
        dbg_flush();

        disable_irq();
        shared_vars.apps[shared_vars.default_app_index].entry_point();
    }

    return 0;
}