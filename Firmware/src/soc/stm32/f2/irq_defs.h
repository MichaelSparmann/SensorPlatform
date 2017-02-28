// Generic Microcontroller Firmware Platform
// Copyright (C) 2017 Michael Sparmann
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.


CORTEXM_DEFINE_IRQ(wwdg)
CORTEXM_DEFINE_IRQ(pvd)
CORTEXM_DEFINE_IRQ(tamp_stamp)
CORTEXM_DEFINE_IRQ(rtc_wkup)
CORTEXM_DEFINE_IRQ(flash)
CORTEXM_DEFINE_IRQ(rcc)
CORTEXM_DEFINE_IRQ(exti0)
CORTEXM_DEFINE_IRQ(exti1)
CORTEXM_DEFINE_IRQ(exti2)
CORTEXM_DEFINE_IRQ(exti3)
CORTEXM_DEFINE_IRQ(exti4)
CORTEXM_DEFINE_IRQ(dma1_stream0)
CORTEXM_DEFINE_IRQ(dma1_stream1)
CORTEXM_DEFINE_IRQ(dma1_stream2)
CORTEXM_DEFINE_IRQ(dma1_stream3)
CORTEXM_DEFINE_IRQ(dma1_stream4)
CORTEXM_DEFINE_IRQ(dma1_stream5)
CORTEXM_DEFINE_IRQ(dma1_stream6)
CORTEXM_DEFINE_IRQ(adc)
CORTEXM_DEFINE_IRQ(can1_tx)
CORTEXM_DEFINE_IRQ(can1_rx0)
CORTEXM_DEFINE_IRQ(can1_rx1)
CORTEXM_DEFINE_IRQ(can1_sce)
CORTEXM_DEFINE_IRQ(exti9_5)
CORTEXM_DEFINE_IRQ(tim1_brk_tim9)
CORTEXM_DEFINE_IRQ(tim1_up_tim10)
CORTEXM_DEFINE_IRQ(tim1_trg_com_tim11)
CORTEXM_DEFINE_IRQ(tim1_cc)
CORTEXM_DEFINE_IRQ(tim2)
CORTEXM_DEFINE_IRQ(tim3)
CORTEXM_DEFINE_IRQ(tim4)
CORTEXM_DEFINE_IRQ(i2c1_ev)
CORTEXM_DEFINE_IRQ(i2c1_er)
CORTEXM_DEFINE_IRQ(i2c2_ev)
CORTEXM_DEFINE_IRQ(i2c2_er)
CORTEXM_DEFINE_IRQ(spi1)
CORTEXM_DEFINE_IRQ(spi2)
CORTEXM_DEFINE_IRQ(usart1)
CORTEXM_DEFINE_IRQ(usart2)
CORTEXM_DEFINE_IRQ(usart3)
CORTEXM_DEFINE_IRQ(exti15_10)
CORTEXM_DEFINE_IRQ(rtc_alarm)
CORTEXM_DEFINE_IRQ(otg_fs_wkup)
CORTEXM_DEFINE_IRQ(tim8_brk_tim12)
CORTEXM_DEFINE_IRQ(tim8_up_tim13)
CORTEXM_DEFINE_IRQ(tim8_trg_com_tim14)
CORTEXM_DEFINE_IRQ(tim8_cc)
CORTEXM_DEFINE_IRQ(dma1_stream7)
CORTEXM_DEFINE_IRQ(fsmc)
CORTEXM_DEFINE_IRQ(sdio)
CORTEXM_DEFINE_IRQ(tim5)
CORTEXM_DEFINE_IRQ(spi3)
CORTEXM_DEFINE_IRQ(uart4)
CORTEXM_DEFINE_IRQ(uart5)
CORTEXM_DEFINE_IRQ(tim6_dac)
CORTEXM_DEFINE_IRQ(tim7)
CORTEXM_DEFINE_IRQ(dma2_stream0)
CORTEXM_DEFINE_IRQ(dma2_stream1)
CORTEXM_DEFINE_IRQ(dma2_stream2)
CORTEXM_DEFINE_IRQ(dma2_stream3)
CORTEXM_DEFINE_IRQ(dma2_stream4)
CORTEXM_DEFINE_IRQ(eth)
CORTEXM_DEFINE_IRQ(eth_wkup)
CORTEXM_DEFINE_IRQ(can2_tx)
CORTEXM_DEFINE_IRQ(can2_rx0)
CORTEXM_DEFINE_IRQ(can2_rx1)
CORTEXM_DEFINE_IRQ(can2_sce)
CORTEXM_DEFINE_IRQ(otg_fs)
#if defined(SOC_STM32F2) || defined(SOC_STM32F4)
CORTEXM_DEFINE_IRQ(dma2_stream5)
CORTEXM_DEFINE_IRQ(dma2_stream6)
CORTEXM_DEFINE_IRQ(dma2_stream7)
CORTEXM_DEFINE_IRQ(usart6)
CORTEXM_DEFINE_IRQ(i2c3_ev)
CORTEXM_DEFINE_IRQ(i2c3_er)
CORTEXM_DEFINE_IRQ(otg_hs_ep1_out)
CORTEXM_DEFINE_IRQ(otg_hs_ep1_in)
CORTEXM_DEFINE_IRQ(otg_hs_wkup)
CORTEXM_DEFINE_IRQ(otg_hs)
CORTEXM_DEFINE_IRQ(dcmi)
CORTEXM_DEFINE_IRQ(cryp)
CORTEXM_DEFINE_IRQ(hash_rng)
#endif
#if defined(SOC_STM32F4)
CORTEXM_DEFINE_IRQ(fpu)
#endif

