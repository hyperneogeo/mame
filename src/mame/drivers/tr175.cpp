// license:BSD-3-Clause
// copyright-holders:
/***********************************************************************************************************************************

Skeleton driver for Relisys TR-175 II color terminal.

************************************************************************************************************************************/

#include "emu.h"
#include "cpu/m68000/m68000.h"
#include "machine/mc68681.h"
#include "video/ramdac.h"
#include "video/scn2674.h"
#include "screen.h"

class tr175_state : public driver_device
{
public:
	tr175_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
	{ }

	DECLARE_WRITE8_MEMBER(ffec01_w);
	DECLARE_WRITE8_MEMBER(fff000_w);
	DECLARE_READ8_MEMBER(fff400_r);
	SCN2674_DRAW_CHARACTER_MEMBER(draw_character);

	void tr175(machine_config &config);
	void mem_map(address_map &map);
	void ramdac_map(address_map &map);
	void vram_map(address_map &map);
private:
	required_device<cpu_device> m_maincpu;
};

WRITE8_MEMBER(tr175_state::ffec01_w)
{
	logerror("%s: Writing %02X to FFEC01\n", machine().describe_context(), data);
}

WRITE8_MEMBER(tr175_state::fff000_w)
{
	logerror("%s: Writing %02X to FFF000\n", machine().describe_context(), data);
}

READ8_MEMBER(tr175_state::fff400_r)
{
	return 0;
}

void tr175_state::mem_map(address_map &map)
{
	map(0x000000, 0x01ffff).rom().region("maincpu", 0);
	map(0xfe8000, 0xfebfff).ram(); // 8-bit?
	map(0xfefe00, 0xfefedd).nopw(); // 8-bit; cleared at startup
	map(0xff8000, 0xffbfff).ram(); // main RAM
	map(0xff0000, 0xff7fff).ram(); // video RAM?
	map(0xffe000, 0xffe01f).rw("duart", FUNC(scn2681_device::read), FUNC(scn2681_device::write)).umask16(0xff00);
	map(0xffe400, 0xffe40f).rw("avdc", FUNC(scn2674_device::read), FUNC(scn2674_device::write)).umask16(0xff00);
	map(0xffe800, 0xffe805).unmaprw(); //AM_DEVREADWRITE8("pai", um82c11_device, read, write, 0xff00)
	map(0xffec01, 0xffec01).w(this, FUNC(tr175_state::ffec01_w));
	map(0xfff000, 0xfff000).w(this, FUNC(tr175_state::fff000_w));
	map(0xfff400, 0xfff400).r(this, FUNC(tr175_state::fff400_r));
	map(0xfffc01, 0xfffc01).w("ramdac", FUNC(ramdac_device::index_w));
	map(0xfffc03, 0xfffc03).w("ramdac", FUNC(ramdac_device::pal_w));
	map(0xfffc05, 0xfffc05).w("ramdac", FUNC(ramdac_device::mask_w));
}

SCN2674_DRAW_CHARACTER_MEMBER(tr175_state::draw_character)
{
}

void tr175_state::vram_map(address_map &map)
{
	map(0x0000, 0x3fff).nopr();
}

void tr175_state::ramdac_map(address_map &map)
{
	map(0x000, 0x3ff).rw("ramdac", FUNC(ramdac_device::ramdac_pal_r), FUNC(ramdac_device::ramdac_rgb666_w));
}

static INPUT_PORTS_START( tr175 )
INPUT_PORTS_END

MACHINE_CONFIG_START(tr175_state::tr175)
	MCFG_DEVICE_ADD("maincpu", M68000, 12'000'000)
	MCFG_DEVICE_PROGRAM_MAP(mem_map)

	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_RAW_PARAMS(XTAL(28'322'000), 900, 0, 720, 449, 0, 416) // guess
	MCFG_SCREEN_UPDATE_DEVICE("avdc", scn2674_device, screen_update)

	MCFG_DEVICE_ADD("avdc", SCN2674, XTAL(28'322'000) / 18) // guess
	MCFG_SCN2674_INTR_CALLBACK(INPUTLINE("maincpu", M68K_IRQ_2))
	MCFG_SCN2674_CHARACTER_WIDTH(18) // guess
	MCFG_SCN2674_DRAW_CHARACTER_CALLBACK_OWNER(tr175_state, draw_character)
	MCFG_DEVICE_ADDRESS_MAP(0, vram_map)
	MCFG_VIDEO_SET_SCREEN("screen")

	MCFG_DEVICE_ADD("duart", SCN2681, XTAL(11'059'200) / 3) // is this the right clock?
	MCFG_MC68681_IRQ_CALLBACK(INPUTLINE("maincpu", M68K_IRQ_1))

	MCFG_PALETTE_ADD("palette", 0x100)
	MCFG_RAMDAC_ADD("ramdac", ramdac_map, "palette")
MACHINE_CONFIG_END



/**************************************************************************************************************

Relisys TR-175 II.
Chips: MC68000P12, HM82C11C, SCN2681, 3x W24257-70L, KDA0476BCN-66 (RAMDAC), 4 undumped proms, Beeper, Button battery
Crystals: 28.322, 46.448, 11.0592, unknown.
Colour screen (VGA).

***************************************************************************************************************/

ROM_START( tr175 )
	ROM_REGION(0x20000, "maincpu", 0)
	ROM_LOAD16_BYTE( "v6.05.u50", 0x00001, 0x10000, CRC(5a33b6b3) SHA1(d673f50dd88f8a154ddaabe34cfcc9ab91435a4c) )
	ROM_LOAD16_BYTE( "v6.05.u45", 0x00000, 0x10000, CRC(e220befe) SHA1(8402280577e6de4b85843222bbd6b06a3f625b3b) )
ROM_END

COMP( 1982, tr175, 0, 0, tr175, tr175, tr175_state, empty_init, "Relisys", "TR-175 II", MACHINE_IS_SKELETON )
