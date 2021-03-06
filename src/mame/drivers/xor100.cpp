// license:BSD-3-Clause
// copyright-holders:Curt Coder
/****************************************************************************************************

        XOR S-100-12

        XOR Data Science was apparently a 1982 reincorporation of a Huntington Beach-based
        company previously known as Delta Products. At least some of the S-100 boards used
        in XOR's systems were originally developed and documented under the former company
        name.

*****************************************************************************************************

        All input must be in upper case.
        Summary of Monitor commands:

        D xxxx yyyy           = dump memory to screen
        F xxxx yyyy zz        = fill memory from xxxx to yyyy-1 with zz
        G xxxx                = execute program at xxxx
        H xxxx yyyy aa bb...  = Search memory for a string of bytes
        L xxxx                = edit memory (. to exit)
        M xxxx yyyy zzzz      = Move (copy) memory
        V xxxx                = Ascii dump of memory to the screen (cr to continue, space to exit)
        X n                   = Select a bank (0 works, others freeze)

        Note some of the commands are a bit buggy, eg F doesn't fill the last byte

*****************************************************************************************************/

/*

    TODO:

    - cannot boot from floppy (at prompt press ^C, wait, it says 'Drive not ready')
    - honor jumper settings
    - CTC signal header
    - serial printer
    - cassette? (no mention of it in the manuals)

*/


#include "emu.h"
#include "includes/xor100.h"
#include "bus/rs232/rs232.h"

/* Read/Write Handlers */

enum
{
	EPROM_0000 = 0,
	EPROM_F800,
	EPROM_OFF
};

void xor100_state::bankswitch()
{
	address_space &program = m_maincpu->space(AS_PROGRAM);
	int banks = m_ram->size() / 0x10000;

	switch (m_mode)
	{
	case EPROM_0000:
		if (m_bank < banks)
		{
			program.install_write_bank(0x0000, 0xffff, "bank1");
			membank("bank1")->set_entry(1 + m_bank);
		}
		else
		{
			program.unmap_write(0x0000, 0xffff);
		}

		program.install_read_bank(0x0000, 0x07ff, 0xf000, "bank2");
		program.install_read_bank(0xf800, 0xffff, "bank3");
		membank("bank2")->set_entry(0);
		membank("bank3")->set_entry(0);
		break;

	case EPROM_F800:
		if (m_bank < banks)
		{
			program.install_write_bank(0x0000, 0xffff, "bank1");
			program.install_read_bank(0x0000, 0xf7ff, "bank2");
			membank("bank1")->set_entry(1 + m_bank);
			membank("bank2")->set_entry(1 + m_bank);
		}
		else
		{
			program.unmap_write(0x0000, 0xffff);
			program.unmap_read(0x0000, 0xf7ff);
		}

		program.install_read_bank(0xf800, 0xffff, "bank3");
		membank("bank3")->set_entry(0);
		break;

	case EPROM_OFF:
		if (m_bank < banks)
		{
			program.install_write_bank(0x0000, 0xffff, "bank1");
			program.install_read_bank(0x0000, 0xf7ff, "bank2");
			program.install_read_bank(0xf800, 0xffff, "bank3");
			membank("bank1")->set_entry(1 + m_bank);
			membank("bank2")->set_entry(1 + m_bank);
			membank("bank3")->set_entry(1 + m_bank);
		}
		else
		{
			program.unmap_write(0x0000, 0xffff);
			program.unmap_read(0x0000, 0xf7ff);
			program.unmap_read(0xf800, 0xffff);
		}
		break;
	}
}

WRITE8_MEMBER( xor100_state::mmu_w )
{
	/*

	    bit     description

	    0       A16
	    1       A17
	    2       A18
	    3       A19
	    4
	    5
	    6
	    7

	*/

	m_bank = data & 0x07;

	bankswitch();
}

WRITE8_MEMBER( xor100_state::prom_toggle_w )
{
	switch (m_mode)
	{
	case EPROM_OFF: m_mode = EPROM_F800; break;
	case EPROM_F800: m_mode = EPROM_OFF; break;
	}

	bankswitch();
}

READ8_MEMBER( xor100_state::prom_disable_r )
{
	m_mode = EPROM_F800;

	bankswitch();

	return 0xff;
}

WRITE8_MEMBER( xor100_state::baud_w )
{
	m_dbrg->str_w(data & 0x0f);
	m_dbrg->stt_w(data >> 4);
}

READ8_MEMBER( xor100_state::fdc_r )
{
	return m_fdc->gen_r(offset) ^ 0xff;
}

WRITE8_MEMBER( xor100_state::fdc_w )
{
	m_fdc->gen_w(offset, data ^ 0xff);
}

READ8_MEMBER( xor100_state::fdc_wait_r )
{
	/*

	    bit     description

	    0
	    1
	    2
	    3
	    4
	    5
	    6
	    7       FDC IRQ

	*/

	if (!machine().side_effects_disabled())
	{
		if (!m_fdc_irq && !m_fdc_drq)
		{
			fatalerror("Z80 WAIT not supported by MAME core\n");
			m_maincpu->set_input_line(Z80_INPUT_LINE_BOGUSWAIT, ASSERT_LINE);
		}
	}

	return !m_fdc_irq << 7;
}

WRITE8_MEMBER( xor100_state::fdc_dcont_w )
{
	/*

	    bit     description

	    0       DS0
	    1       DS1
	    2       DS2
	    3       DS3
	    4
	    5
	    6
	    7       _HLSTB

	*/

	// drive select
	floppy_image_device *floppy = nullptr;

	if (BIT(data, 0)) floppy = m_floppy0->get_device();
	if (BIT(data, 1)) floppy = m_floppy1->get_device();
	if (BIT(data, 2)) floppy = m_floppy2->get_device();
	if (BIT(data, 3)) floppy = m_floppy3->get_device();

	m_fdc->set_floppy(floppy);

	if (floppy) floppy->mon_w(0);
}

WRITE8_MEMBER( xor100_state::fdc_dsel_w )
{
	/*

	    bit     description

	    0       J
	    1       K
	    2
	    3
	    4
	    5
	    6
	    7

	*/

	switch (data & 0x03)
	{
	case 0: break;
	case 1: m_fdc_dden = 1; break;
	case 2: m_fdc_dden = 0; break;
	case 3: m_fdc_dden = !m_fdc_dden; break;
	}

	m_fdc->dden_w(m_fdc_dden);
}

/* Memory Maps */

void xor100_state::xor100_mem(address_map &map)
{
	map(0x0000, 0xffff).bankw("bank1");
	map(0x0000, 0xf7ff).bankr("bank2");
	map(0xf800, 0xffff).bankr("bank3");
}

void xor100_state::xor100_io(address_map &map)
{
	map.global_mask(0xff);
	map(0x00, 0x00).rw(m_uart_a, FUNC(i8251_device::data_r), FUNC(i8251_device::data_w));
	map(0x01, 0x01).rw(m_uart_a, FUNC(i8251_device::status_r), FUNC(i8251_device::control_w));
	map(0x02, 0x02).rw(m_uart_b, FUNC(i8251_device::data_r), FUNC(i8251_device::data_w));
	map(0x03, 0x03).rw(m_uart_b, FUNC(i8251_device::status_r), FUNC(i8251_device::control_w));
	map(0x04, 0x07).rw(I8255A_TAG, FUNC(i8255_device::read), FUNC(i8255_device::write));
	map(0x08, 0x08).w(this, FUNC(xor100_state::mmu_w));
	map(0x09, 0x09).w(this, FUNC(xor100_state::prom_toggle_w));
	map(0x0a, 0x0a).r(this, FUNC(xor100_state::prom_disable_r));
	map(0x0b, 0x0b).portr("DSW0").w(this, FUNC(xor100_state::baud_w));
	map(0x0c, 0x0f).rw(m_ctc, FUNC(z80ctc_device::read), FUNC(z80ctc_device::write));
	map(0xf8, 0xfb).rw(this, FUNC(xor100_state::fdc_r), FUNC(xor100_state::fdc_w));
	map(0xfc, 0xfc).rw(this, FUNC(xor100_state::fdc_wait_r), FUNC(xor100_state::fdc_dcont_w));
	map(0xfd, 0xfd).w(this, FUNC(xor100_state::fdc_dsel_w));
}

/* Input Ports */

static INPUT_PORTS_START( xor100 )

	PORT_START("DSW0")
	PORT_DIPNAME( 0x0f, 0x05, "Serial Port A" )
	PORT_DIPSETTING(    0x00, "50 baud" )
	PORT_DIPSETTING(    0x01, "75 baud" )
	PORT_DIPSETTING(    0x02, "110 baud" )
	PORT_DIPSETTING(    0x03, "134.5 baud" )
	PORT_DIPSETTING(    0x04, "150 baud" )
	PORT_DIPSETTING(    0x05, "300 baud" )
	PORT_DIPSETTING(    0x06, "600 baud" )
	PORT_DIPSETTING(    0x07, "1200 baud" )
	PORT_DIPSETTING(    0x08, "1800 baud" )
	PORT_DIPSETTING(    0x09, "2000 baud" )
	PORT_DIPSETTING(    0x0a, "2400 baud" )
	PORT_DIPSETTING(    0x0b, "3600 baud" )
	PORT_DIPSETTING(    0x0c, "4800 baud" )
	PORT_DIPSETTING(    0x0d, "7200 baud" )
	PORT_DIPSETTING(    0x0e, "9600 baud" )
	PORT_DIPSETTING(    0x0f, "19200 baud" )
	PORT_DIPNAME( 0xf0, 0xe0, "Serial Port B" )
	PORT_DIPSETTING(    0x00, "50 baud" )
	PORT_DIPSETTING(    0x10, "75 baud" )
	PORT_DIPSETTING(    0x20, "110 baud" )
	PORT_DIPSETTING(    0x30, "134.5 baud" )
	PORT_DIPSETTING(    0x40, "150 baud" )
	PORT_DIPSETTING(    0x50, "300 baud" )
	PORT_DIPSETTING(    0x60, "600 baud" )
	PORT_DIPSETTING(    0x70, "1200 baud" )
	PORT_DIPSETTING(    0x80, "1800 baud" )
	PORT_DIPSETTING(    0x90, "2000 baud" )
	PORT_DIPSETTING(    0xa0, "2400 baud" )
	PORT_DIPSETTING(    0xb0, "3600 baud" )
	PORT_DIPSETTING(    0xc0, "4800 baud" )
	PORT_DIPSETTING(    0xd0, "7200 baud" )
	PORT_DIPSETTING(    0xe0, "9600 baud" )
	PORT_DIPSETTING(    0xf0, "19200 baud" )

	PORT_START("J1")
	PORT_CONFNAME( 0x01, 0x01, "J1 Wait State")
	PORT_CONFSETTING( 0x00, "No Wait States" )
	PORT_CONFSETTING( 0x01, "1 M1 Wait State" )

	PORT_START("J2")
	PORT_CONFNAME( 0x01, 0x01, "J2 CPU Speed")
	PORT_CONFSETTING( 0x00, "2 MHz" )
	PORT_CONFSETTING( 0x01, "4 MHz" )

	PORT_START("J3")
	PORT_CONFNAME( 0x01, 0x00, "J3")
	PORT_CONFSETTING( 0x00, "" )
	PORT_CONFSETTING( 0x01, "" )

	PORT_START("J4-J5")
	PORT_CONFNAME( 0x01, 0x01, "J4/J5 EPROM Type")
	PORT_CONFSETTING( 0x00, "2708" )
	PORT_CONFSETTING( 0x01, "2716" )

	PORT_START("J6")
	PORT_CONFNAME( 0x01, 0x01, "J6 EPROM")
	PORT_CONFSETTING( 0x00, "Disabled" )
	PORT_CONFSETTING( 0x01, "Enabled" )

	PORT_START("J7")
	PORT_CONFNAME( 0x01, 0x00, "J7 I/O Port Addresses")
	PORT_CONFSETTING( 0x00, "00-0F" )
	PORT_CONFSETTING( 0x01, "10-1F" )

	PORT_START("J8")
	PORT_CONFNAME( 0x01, 0x00, "J8")
	PORT_CONFSETTING( 0x00, "" )
	PORT_CONFSETTING( 0x01, "" )

	PORT_START("J9")
	PORT_CONFNAME( 0x01, 0x01, "J9 Power On RAM")
	PORT_CONFSETTING( 0x00, "Enabled" )
	PORT_CONFSETTING( 0x01, "Disabled" )
INPUT_PORTS_END

/* Printer 8255A Interface */

WRITE_LINE_MEMBER( xor100_state::write_centronics_busy )
{
	m_centronics_busy = state;
}

WRITE_LINE_MEMBER( xor100_state::write_centronics_select )
{
	m_centronics_select = state;
}

READ8_MEMBER(xor100_state::i8255_pc_r)
{
	/*

	    bit     description

	    PC0
	    PC1
	    PC2
	    PC3
	    PC4     ON LINE
	    PC5     BUSY
	    PC6     _ACK
	    PC7

	*/

	uint8_t data = 0;

	/* on line */
	data |= m_centronics_select << 4;

	/* busy */
	data |= m_centronics_busy << 5;

	return data;
}

/* Z80-CTC Interface */

WRITE_LINE_MEMBER( xor100_state::ctc_z0_w )
{
}

WRITE_LINE_MEMBER( xor100_state::ctc_z1_w )
{
}

WRITE_LINE_MEMBER( xor100_state::ctc_z2_w )
{
}

/* WD1795-02 Interface */

static void xor100_floppies(device_slot_interface &device)
{
	device.option_add("8ssdd", FLOPPY_8_SSDD); // Shugart SA-100
}

void xor100_state::fdc_intrq_w(bool state)
{
	m_fdc_irq = state;
	m_ctc->trg0(state);

	if (state)
	{
		fatalerror("Z80 WAIT not supported by MAME core\n");
		m_maincpu->set_input_line(Z80_INPUT_LINE_BOGUSWAIT, ASSERT_LINE);
	}
}

void xor100_state::fdc_drq_w(bool state)
{
	m_fdc_drq = state;

	if (state)
	{
		fatalerror("Z80 WAIT not supported by MAME core\n");
		m_maincpu->set_input_line(Z80_INPUT_LINE_BOGUSWAIT, ASSERT_LINE);
	}
}


static DEVICE_INPUT_DEFAULTS_START( terminal )
	DEVICE_INPUT_DEFAULTS( "RS232_TXBAUD", 0xff, RS232_BAUD_9600 )
	DEVICE_INPUT_DEFAULTS( "RS232_RXBAUD", 0xff, RS232_BAUD_9600 )
	DEVICE_INPUT_DEFAULTS( "RS232_STARTBITS", 0xff, RS232_STARTBITS_1 )
	DEVICE_INPUT_DEFAULTS( "RS232_DATABITS", 0xff, RS232_DATABITS_8 )
	DEVICE_INPUT_DEFAULTS( "RS232_PARITY", 0xff, RS232_PARITY_NONE )
	DEVICE_INPUT_DEFAULTS( "RS232_STOPBITS", 0xff, RS232_STOPBITS_1 )
DEVICE_INPUT_DEFAULTS_END

static void xor100_s100_cards(device_slot_interface &device)
{
}

/* Machine Initialization */

void xor100_state::machine_start()
{
	int banks = m_ram->size() / 0x10000;
	uint8_t *ram = m_ram->pointer();
	uint8_t *rom = m_rom->base();

	/* setup memory banking */
	membank("bank1")->configure_entries(1, banks, ram, 0x10000);
	membank("bank2")->configure_entry(0, rom);
	membank("bank2")->configure_entries(1, banks, ram, 0x10000);
	membank("bank3")->configure_entry(0, rom);
	membank("bank3")->configure_entries(1, banks, ram + 0xf800, 0x10000);

	machine().save().register_postload(save_prepost_delegate(FUNC(xor100_state::post_load), this));

	/* register for state saving */
	save_item(NAME(m_mode));
	save_item(NAME(m_bank));
	save_item(NAME(m_fdc_irq));
	save_item(NAME(m_fdc_drq));
	save_item(NAME(m_fdc_dden));
	save_item(NAME(m_centronics_busy));
	save_item(NAME(m_centronics_select));
}

void xor100_state::machine_reset()
{
	m_mode = EPROM_0000;

	bankswitch();
}

void xor100_state::post_load()
{
	bankswitch();
}

/* Machine Driver */

MACHINE_CONFIG_START(xor100_state::xor100)
	/* basic machine hardware */
	MCFG_DEVICE_ADD(Z80_TAG, Z80, 8_MHz_XTAL / 2)
	MCFG_DEVICE_PROGRAM_MAP(xor100_mem)
	MCFG_DEVICE_IO_MAP(xor100_io)

	/* devices */
	MCFG_DEVICE_ADD(I8251_A_TAG, I8251, 0/*8_MHz_XTAL / 2,*/)
	MCFG_I8251_TXD_HANDLER(WRITELINE(RS232_A_TAG, rs232_port_device, write_txd))
	MCFG_I8251_DTR_HANDLER(WRITELINE(RS232_A_TAG, rs232_port_device, write_dtr))
	MCFG_I8251_RTS_HANDLER(WRITELINE(RS232_A_TAG, rs232_port_device, write_rts))

	MCFG_DEVICE_ADD(RS232_A_TAG, RS232_PORT, default_rs232_devices, nullptr)
	MCFG_RS232_RXD_HANDLER(WRITELINE(I8251_A_TAG, i8251_device, write_rxd))
	MCFG_RS232_DSR_HANDLER(WRITELINE(I8251_A_TAG, i8251_device, write_dsr))

	MCFG_DEVICE_ADD(I8251_B_TAG, I8251, 0/*8_MHz_XTAL / 2,*/)
	MCFG_I8251_TXD_HANDLER(WRITELINE(RS232_B_TAG, rs232_port_device, write_txd))
	MCFG_I8251_DTR_HANDLER(WRITELINE(RS232_B_TAG, rs232_port_device, write_dtr))
	MCFG_I8251_RTS_HANDLER(WRITELINE(RS232_B_TAG, rs232_port_device, write_rts))

	MCFG_DEVICE_ADD(RS232_B_TAG, RS232_PORT, default_rs232_devices, "terminal")
	MCFG_RS232_RXD_HANDLER(WRITELINE(I8251_B_TAG, i8251_device, write_rxd))
	MCFG_RS232_DSR_HANDLER(WRITELINE(I8251_B_TAG, i8251_device, write_dsr))
	MCFG_RS232_CTS_HANDLER(WRITELINE(I8251_B_TAG, i8251_device, write_cts))
	MCFG_SLOT_OPTION_DEVICE_INPUT_DEFAULTS("terminal", terminal)

	MCFG_DEVICE_ADD(COM5016_TAG, COM8116, 5.0688_MHz_XTAL)
	MCFG_COM8116_FR_HANDLER(WRITELINE(I8251_A_TAG, i8251_device, write_txc))
	MCFG_DEVCB_CHAIN_OUTPUT(WRITELINE(I8251_A_TAG, i8251_device, write_rxc))
	MCFG_COM8116_FT_HANDLER(WRITELINE(I8251_B_TAG, i8251_device, write_txc))
	MCFG_DEVCB_CHAIN_OUTPUT(WRITELINE(I8251_B_TAG, i8251_device, write_rxc))

	MCFG_DEVICE_ADD(I8255A_TAG, I8255A, 0)
	MCFG_I8255_OUT_PORTA_CB(WRITE8("cent_data_out", output_latch_device, write))
	MCFG_I8255_OUT_PORTB_CB(WRITELINE(CENTRONICS_TAG, centronics_device, write_strobe))
	MCFG_I8255_IN_PORTC_CB(READ8(*this, xor100_state, i8255_pc_r))

	MCFG_DEVICE_ADD(Z80CTC_TAG, Z80CTC, 8_MHz_XTAL / 2)
	MCFG_Z80CTC_INTR_CB(INPUTLINE(Z80_TAG, INPUT_LINE_IRQ0))
	MCFG_Z80CTC_ZC0_CB(WRITELINE(*this, xor100_state, ctc_z0_w))
	MCFG_Z80CTC_ZC1_CB(WRITELINE(*this, xor100_state, ctc_z1_w))
	MCFG_Z80CTC_ZC2_CB(WRITELINE(*this, xor100_state, ctc_z2_w))

	MCFG_DEVICE_ADD(WD1795_TAG, FD1795, 8_MHz_XTAL / 4)
	MCFG_FLOPPY_DRIVE_ADD(WD1795_TAG":0", xor100_floppies, "8ssdd", floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(WD1795_TAG":1", xor100_floppies, "8ssdd", floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(WD1795_TAG":2", xor100_floppies, nullptr,    floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD(WD1795_TAG":3", xor100_floppies, nullptr,    floppy_image_device::default_floppy_formats)

	MCFG_CENTRONICS_ADD(CENTRONICS_TAG, centronics_devices, "printer")
	MCFG_CENTRONICS_ACK_HANDLER(WRITELINE(I8255A_TAG, i8255_device, pc4_w))
	MCFG_CENTRONICS_BUSY_HANDLER(WRITELINE(*this, xor100_state, write_centronics_busy))
	MCFG_CENTRONICS_SELECT_HANDLER(WRITELINE(*this, xor100_state, write_centronics_select))

	MCFG_CENTRONICS_OUTPUT_LATCH_ADD("cent_data_out", "centronics")

	// S-100
	MCFG_DEVICE_ADD(S100_TAG, S100_BUS, 8_MHz_XTAL / 4)
	MCFG_S100_RDY_CALLBACK(INPUTLINE(Z80_TAG, Z80_INPUT_LINE_BOGUSWAIT))
	MCFG_S100_SLOT_ADD(S100_TAG ":1", xor100_s100_cards, nullptr)
	MCFG_S100_SLOT_ADD(S100_TAG ":2", xor100_s100_cards, nullptr)
	MCFG_S100_SLOT_ADD(S100_TAG ":3", xor100_s100_cards, nullptr)
	MCFG_S100_SLOT_ADD(S100_TAG ":4", xor100_s100_cards, nullptr)
	MCFG_S100_SLOT_ADD(S100_TAG ":5", xor100_s100_cards, nullptr)
	MCFG_S100_SLOT_ADD(S100_TAG ":6", xor100_s100_cards, nullptr)
	MCFG_S100_SLOT_ADD(S100_TAG ":7", xor100_s100_cards, nullptr)
	MCFG_S100_SLOT_ADD(S100_TAG ":8", xor100_s100_cards, nullptr)
	MCFG_S100_SLOT_ADD(S100_TAG ":9", xor100_s100_cards, nullptr)
	MCFG_S100_SLOT_ADD(S100_TAG ":10", xor100_s100_cards, nullptr)

	/* internal ram */
	MCFG_RAM_ADD(RAM_TAG)
	MCFG_RAM_DEFAULT_SIZE("64K")
	MCFG_RAM_EXTRA_OPTIONS("128K,192K,256K,320K,384K,448K,512K")
MACHINE_CONFIG_END

/* ROMs */

ROM_START( xor100 )
	ROM_REGION( 0x800, Z80_TAG, 0 )
	ROM_SYSTEM_BIOS( 0, "v185", "v1.85" )
	ROMX_LOAD( "xp 185.8b", 0x000, 0x800, CRC(0d0bda8d) SHA1(11c83f7cd7e6a570641b44a2f2cc5737a7dd8ae3), ROM_BIOS(0) )
ROM_END

/* System Drivers */

//    YEAR  NAME    PARENT  COMPAT  MACHINE  INPUT   STATE         INIT        COMPANY             FULLNAME        FLAGS
COMP( 1980, xor100, 0,      0,      xor100,  xor100, xor100_state, empty_init, "XOR Data Science", "XOR S-100-12", MACHINE_SUPPORTS_SAVE | MACHINE_NO_SOUND_HW )
