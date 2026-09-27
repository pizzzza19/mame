// license:BSD-3-Clause
// copyright-holders:superctr, Valley Bell
/***************************************************************************

  Capcom QSound DL-1425 (HLE)
  ===========================

  Driver by superctr with thanks to Valley Bell.

  Based on disassembled DSP code.

  Links:
  https://siliconpr0n.org/map/capcom/dl-1425

***************************************************************************/

#include "emu.h"

#ifndef QSOUND_LLE
#define QSOUND_LLE
#endif

#include "qsoundhle.h"

#include "qsound.h"

#include <algorithm>
#include <limits>

// ============================================================================
// ハードコードされたDSP ROMテーブル（FBNeoより移植）
// ============================================================================

// ドライミックステーブル（パンニング用）
static const int16_t qsound_dry_mix_table[33] = {
	-16384,-16384,-16384,-16384,-16384,-16384,-16384,-16384,
	-16384,-16384,-16384,-16384,-16384,-16384,-16384,-16384,
	-16384,-14746,-13107,-11633,-10486,-9175,-8520,-7209,
	-6226,-5226,-4588,-3768,-3277,-2703,-2130,-1802,
	0
};

// ウェットミックステーブル（パンニング用）
static const int16_t qsound_wet_mix_table[33] = {
	0,-1638,-1966,-2458,-2949,-3441,-4096,-4669,
	-4915,-5120,-5489,-6144,-7537,-8831,-9339,-9830,
	-10240,-10322,-10486,-10568,-10650,-11796,-12288,-12288,
	-12534,-12648,-12780,-12829,-12943,-13107,-13418,-14090,
	-16384
};

// リニアミックステーブル（リニアパンニング用）
static const int16_t qsound_linear_mix_table[33] = {
	-16379,-16338,-16257,-16135,-15973,-15772,-15531,-15251,
	-14934,-14580,-14189,-13763,-13303,-12810,-12284,-11729,
	-11729,-11144,-10531,-9893,-9229,-8543,-7836,-7109,
	-6364,-5604,-4829,-4043,-3246,-2442,-1631,-817,
	0
};

// FIRフィルター係数テーブル（5種類、各95タップ）
static const int16_t qsound_filter_data[5][95] = {
	{	// テーブル0（アドレス 0xd53）
		0,0,0,6,44,-24,-53,-10,59,-40,-27,1,39,-27,56,127,174,36,-13,49,
		212,142,143,-73,-20,66,-108,-117,-399,-265,-392,-569,-473,-71,95,-319,-218,-230,331,638,
		449,477,-180,532,1107,750,9899,3828,-2418,1071,-176,191,-431,64,117,-150,-274,-97,-238,165,
		166,250,-19,4,37,204,186,-6,140,-77,-1,1,18,-10,-151,-149,-103,-9,55,23,
		-102,-97,-11,13,-48,-27,5,18,-61,-30,64,72,0,0,0,
	},
	{	// テーブル1（アドレス 0xdb2）- デフォルト左チャンネルフィルター
		0,0,0,85,24,-76,-123,-86,-29,-14,-20,-7,6,-28,-87,-89,-5,100,154,160,
		150,118,41,-48,-78,-23,59,83,-2,-176,-333,-344,-203,-66,-39,2,224,495,495,280,
		432,1340,2483,5377,1905,658,0,97,347,285,35,-95,-78,-82,-151,-192,-171,-149,-147,-113,
		-22,71,118,129,127,110,71,31,20,36,46,23,-27,-63,-53,-21,-19,-60,-92,-69,
		-12,25,29,30,40,41,29,30,46,39,-15,-74,0,0,0,
	},
	{	// テーブル2（アドレス 0xe11）- デフォルト右チャンネルフィルター
		0,0,0,23,42,47,29,10,2,-14,-54,-92,-93,-70,-64,-77,-57,18,94,113,
		87,69,67,50,25,29,58,62,24,-39,-131,-256,-325,-234,-45,58,78,223,485,496,
		127,6,857,2283,2683,4928,1328,132,79,314,189,-80,-90,35,-21,-186,-195,-99,-136,-258,
		-189,82,257,185,53,41,84,68,38,63,77,14,-60,-71,-71,-120,-151,-84,14,29,
		-8,7,66,69,12,-3,54,92,52,-6,-15,-2,0,0,0,
	},
	{	// テーブル3（アドレス 0xe70）
		0,0,0,2,-28,-37,-17,0,-9,-22,-3,35,52,39,20,7,-6,2,55,121,
		129,67,8,1,9,-6,-16,16,66,96,118,130,75,-47,-92,43,223,239,151,219,
		440,475,226,206,940,2100,2663,4980,865,49,-33,186,231,103,42,114,191,184,116,29,
		-47,-72,-21,60,96,68,31,32,63,87,76,39,7,14,55,85,67,18,-12,-3,
		21,34,29,6,-27,-49,-37,-2,16,0,-21,-16,0,0,0,
	},
	{	// テーブル4（アドレス 0xecf）
		0,0,0,48,7,-22,-29,-10,24,54,59,29,-36,-117,-185,-213,-185,-99,13,90,
		83,24,-5,23,53,47,38,56,67,57,75,107,16,-242,-440,-355,-120,-33,-47,152,
		501,472,-57,-292,544,1937,2277,6145,1240,153,47,200,152,36,64,134,74,-82,-208,-266,
		-268,-188,-42,65,74,56,89,133,114,44,-3,-1,17,29,29,-2,-76,-156,-187,-151,
		-85,-31,-5,7,20,32,24,-5,-20,6,48,62,0,0,0,
	}
};

// モード2および特殊フィルター用データ（209値）
static const int16_t qsound_filter_data2[209] = {
	// アドレス 0xf2e - 出力無効化フィルター（95個のゼロ）
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,
	// アドレス 0xf73 - モード2フィルター（45値）
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,
	-371,-196,-268,-512,-303,-315,-184,-76,276,-256,298,196,990,236,1114,-126,4377,6549,791,
	// アドレス 0xfa0 - フィルタリング無効（中央タップ = -16384）
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,-16384,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

// ADPCMステップサイズテーブル
static const int16_t adpcm_step_table[16] = {
	154, 154, 128, 102, 77, 58, 58, 58,
	58, 58, 58, 58, 77, 102, 128, 154
};


// device type definition
DEFINE_DEVICE_TYPE(QSOUND_HLE, qsound_hle_device, "qsound_hle", "QSound (HLE)")


//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  qsound_hle_device - constructor
//-------------------------------------------------

qsound_hle_device::qsound_hle_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, QSOUND_HLE, tag, owner, clock)
	, device_sound_interface(mconfig, *this)
	, device_rom_interface(mconfig, *this)
	, m_stream(nullptr)
	, m_data_latch(0)
{
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void qsound_hle_device::device_start()
{
	m_stream = stream_alloc(0, 2, clock() / 2 / 1248); // DSP program uses 1248 machine cycles per iteration

	init_register_map();

	// state save
	// PCM registers
	// PCM voices
	save_item(STRUCT_MEMBER(m_voice, m_bank));
	save_item(STRUCT_MEMBER(m_voice, m_addr));
	save_item(STRUCT_MEMBER(m_voice, m_phase));
	save_item(STRUCT_MEMBER(m_voice, m_rate));
	save_item(STRUCT_MEMBER(m_voice, m_loop_len));
	save_item(STRUCT_MEMBER(m_voice, m_end_addr));
	save_item(STRUCT_MEMBER(m_voice, m_volume));
	save_item(STRUCT_MEMBER(m_voice, m_echo));

	// ADPCM voices
	save_item(STRUCT_MEMBER(m_adpcm, m_start_addr));
	save_item(STRUCT_MEMBER(m_adpcm, m_end_addr));
	save_item(STRUCT_MEMBER(m_adpcm, m_bank));
	save_item(STRUCT_MEMBER(m_adpcm, m_volume));
	save_item(STRUCT_MEMBER(m_adpcm, m_flag));
	save_item(STRUCT_MEMBER(m_adpcm, m_cur_vol));
	save_item(STRUCT_MEMBER(m_adpcm, m_step_size));
	save_item(STRUCT_MEMBER(m_adpcm, m_cur_addr));

	// PCM voices
	save_item(NAME(m_voice_pan));

	// QSound registers
	save_item(NAME(m_echo.m_end_pos));
	save_item(NAME(m_echo.m_feedback));
	save_item(NAME(m_echo.m_length));
	save_item(NAME(m_echo.m_last_sample));
	save_item(NAME(m_echo.m_delay_line));
	save_item(NAME(m_echo.m_delay_pos));

	// left, right
	save_item(STRUCT_MEMBER(m_filter, m_tap_count));
	save_item(STRUCT_MEMBER(m_filter, m_delay_pos));
	save_item(STRUCT_MEMBER(m_filter, m_table_pos));
	save_item(STRUCT_MEMBER(m_filter, m_taps));
	save_item(STRUCT_MEMBER(m_filter, m_delay_line));

	save_item(STRUCT_MEMBER(m_alt_filter, m_tap_count));
	save_item(STRUCT_MEMBER(m_alt_filter, m_delay_pos));
	save_item(STRUCT_MEMBER(m_alt_filter, m_table_pos));
	save_item(STRUCT_MEMBER(m_alt_filter, m_taps));
	save_item(STRUCT_MEMBER(m_alt_filter, m_delay_line));

	save_item(STRUCT_MEMBER(m_wet, m_delay));
	save_item(STRUCT_MEMBER(m_wet, m_volume));
	save_item(STRUCT_MEMBER(m_wet, m_write_pos));
	save_item(STRUCT_MEMBER(m_wet, m_read_pos));
	save_item(STRUCT_MEMBER(m_wet, m_delay_line));

	save_item(STRUCT_MEMBER(m_dry, m_delay));
	save_item(STRUCT_MEMBER(m_dry, m_volume));
	save_item(STRUCT_MEMBER(m_dry, m_write_pos));
	save_item(STRUCT_MEMBER(m_dry, m_read_pos));
	save_item(STRUCT_MEMBER(m_dry, m_delay_line));

	save_item(NAME(m_state));
	save_item(NAME(m_next_state));
	save_item(NAME(m_delay_update));
	save_item(NAME(m_state_counter));
	save_item(NAME(m_ready_flag));
	save_item(NAME(m_data_latch));
	save_item(NAME(m_out));
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void qsound_hle_device::device_reset()
{
	m_ready_flag = 0;
	m_out[0] = m_out[1] = 0;
	m_state = STATE_BOOT;
	m_state_counter = 0;
}

//-------------------------------------------------
//  sound_stream_update - handle a stream update
//-------------------------------------------------

void qsound_hle_device::sound_stream_update(sound_stream &stream)
{
	for (int i = 0; i < stream.samples(); i ++)
	{
		update_sample();
		stream.put_int(0, i, m_out[0], 32768);
		stream.put_int(1, i, m_out[1], 32768);
	}
}


void qsound_hle_device::qsound_w(offs_t offset, uint8_t data)
{
	switch (offset)
	{
		case 0:
			m_data_latch = (m_data_latch & 0x00ff) | (data << 8);
			break;

		case 1:
			m_data_latch = (m_data_latch & 0xff00) | data;
			break;

		case 2:
			m_stream->update();
			write_data(data, m_data_latch);
			break;

		default:
			logerror("%s: qsound_w %d = %02x\n", machine().describe_context(), offset, data);
			break;
	}
}


uint8_t qsound_hle_device::qsound_r()
{
	// ready bit (0x00 = busy, 0x80 == ready)
	m_stream->update();
	return m_ready_flag;
}


void qsound_hle_device::write_data(uint8_t address, uint16_t data)
{
	uint16_t *destination = m_register_map[address];
	if (destination)
		*destination = data;
	m_ready_flag = 0;
}

void qsound_hle_device::init_register_map()
{
	// unused registers
	std::fill(std::begin(m_register_map), std::end(m_register_map), nullptr);

	// PCM registers
	for (int i = 0; i < 16; i++) // PCM voices
	{
		m_register_map[(i << 3) + 0] = (uint16_t*)&m_voice[(i + 1) % 16].m_bank; // Bank applies to the next channel
		m_register_map[(i << 3) + 1] = (uint16_t*)&m_voice[i].m_addr; // Current sample position and start position.
		m_register_map[(i << 3) + 2] = (uint16_t*)&m_voice[i].m_rate; // 4.12 fixed point decimal.
		m_register_map[(i << 3) + 3] = (uint16_t*)&m_voice[i].m_phase;
		m_register_map[(i << 3) + 4] = (uint16_t*)&m_voice[i].m_loop_len;
		m_register_map[(i << 3) + 5] = (uint16_t*)&m_voice[i].m_end_addr;
		m_register_map[(i << 3) + 6] = (uint16_t*)&m_voice[i].m_volume;
		m_register_map[(i << 3) + 7] = nullptr; // unused
		m_register_map[i + 0x80] = (uint16_t*)&m_voice_pan[i];
		m_register_map[i + 0xba] = (uint16_t*)&m_voice[i].m_echo;
	}

	// ADPCM registers
	for (int i = 0; i < 3; i++) // ADPCM voices
	{
		// ADPCM sample rate is fixed to 8khz. (one channel is updated every third sample)
		m_register_map[(i << 2) + 0xca] = (uint16_t*)&m_adpcm[i].m_start_addr;
		m_register_map[(i << 2) + 0xcb] = (uint16_t*)&m_adpcm[i].m_end_addr;
		m_register_map[(i << 2) + 0xcc] = (uint16_t*)&m_adpcm[i].m_bank;
		m_register_map[(i << 2) + 0xcd] = (uint16_t*)&m_adpcm[i].m_volume;
		m_register_map[i + 0xd6] = (uint16_t*)&m_adpcm[i].m_flag; // non-zero to start ADPCM playback
		m_register_map[i + 0x90] = (uint16_t*)&m_voice_pan[16 + i];
	}

	// QSound registers
	m_register_map[0x93] = (uint16_t*)&m_echo.m_feedback;
	m_register_map[0xd9] = (uint16_t*)&m_echo.m_end_pos;
	m_register_map[0xe2] = (uint16_t*)&m_delay_update; // non-zero to update delays
	m_register_map[0xe3] = (uint16_t*)&m_next_state;
	for (int i = 0; i < 2; i++)  // left, right
	{
		// Wet
		m_register_map[(i << 1) + 0xda] = (uint16_t*)&m_filter[i].m_table_pos;
		m_register_map[(i << 1) + 0xde] = (uint16_t*)&m_wet[i].m_delay;
		m_register_map[(i << 1) + 0xe4] = (uint16_t*)&m_wet[i].m_volume;
		// Dry
		m_register_map[(i << 1) + 0xdb] = (uint16_t*)&m_alt_filter[i].m_table_pos;
		m_register_map[(i << 1) + 0xdf] = (uint16_t*)&m_dry[i].m_delay;
		m_register_map[(i << 1) + 0xe5] = (uint16_t*)&m_dry[i].m_volume;
	}
}

int16_t qsound_hle_device::read_sample(uint16_t bank, uint16_t address)
{
	bank &= 0x7fff;
	const uint32_t rom_addr = (bank << 16) | (address << 0);
	const uint8_t sample_data = read_byte(rom_addr);
	return (int16_t)(sample_data << 8); // bit0-7 is tied to ground
}

int16_t qsound_hle_device::read_dsp_rom(uint16_t offset)
{
	// フィルター係数（モード1用、5テーブル × 95タップ）
	if (offset >= 0xd53 && offset < 0xd53 + (5 * 95))
	{
		int table_index = (offset - 0xd53) / 95;
		int tap_index = (offset - 0xd53) % 95;
		return qsound_filter_data[table_index][tap_index];
	}

	// フィルターデータ2（モード2および特殊フィルター）
	if (offset >= 0xf2e && offset < 0xfff)
	{
		int index = offset - 0xf2e;
		if (index < 209)
			return qsound_filter_data2[index];
	}

	// パンテーブル
	if (offset >= DATA_PAN_TAB)
	{
		int pan_offset = offset - DATA_PAN_TAB;
		int channel = pan_offset / 196;
		int local_offset = pan_offset % 196;
		
		bool is_wet = (local_offset >= 98);
		int pan_index = is_wet ? (local_offset - 98) : local_offset;
		
		if (pan_index <= 32)
		{
			int table_idx = (channel == 0) ? pan_index : (32 - pan_index);
			return is_wet ? qsound_wet_mix_table[table_idx] : qsound_dry_mix_table[table_idx];
		}
		else if (pan_index >= 48 && pan_index <= 80)
		{
			int linear_idx = pan_index - 48;
			if (linear_idx <= 32)
			{
				int table_idx = (channel == 0) ? linear_idx : (32 - linear_idx);
				return is_wet ? 0 : qsound_linear_mix_table[table_idx];
			}
		}
	}

	// ADPCMステップテーブル
	if (offset >= DATA_ADPCM_TAB + 8 && offset < DATA_ADPCM_TAB + 24)
	{
		int index = offset - (DATA_ADPCM_TAB + 8);
		if (index >= 0 && index < 16)
			return adpcm_step_table[index];
	}

	return 0;
}

/********************************************************************/

// updates one DSP sample
void qsound_hle_device::update_sample()
{
	switch (m_state)
	{
		default:
		case STATE_INIT1:
		case STATE_INIT2:
			return state_init();
		case STATE_REFRESH1:
			return state_refresh_filter_1();
		case STATE_REFRESH2:
			return state_refresh_filter_2();
		case STATE_NORMAL1:
		case STATE_NORMAL2:
			return state_normal_update();
	}
}

// Initialization routine
void qsound_hle_device::state_init()
{
	int mode = (m_state == STATE_INIT2) ? 1 : 0;

	// we're busy for 4 samples, including the filter refresh.
	if (m_state_counter >= 2)
	{
		m_state_counter = 0;
		m_state = m_next_state;
		return;
	}
	else if (m_state_counter == 1)
	{
		m_state_counter++;
		return;
	}

	std::fill(std::begin(m_voice), std::end(m_voice), qsound_voice());
	std::fill(std::begin(m_adpcm), std::end(m_adpcm), qsound_adpcm());
	std::fill(std::begin(m_filter), std::end(m_filter), qsound_fir());
	std::fill(std::begin(m_alt_filter), std::end(m_alt_filter), qsound_fir());
	std::fill(std::begin(m_wet), std::end(m_wet), qsound_delay());
	std::fill(std::begin(m_dry), std::end(m_dry), qsound_delay());
	m_echo = qsound_echo();

	for (int i = 0; i < 19; i++)
	{
		m_voice_pan[i] = DATA_PAN_TAB + 0x10;
		m_voice_output[i] = 0;
	}

	for (int i = 0; i < 16; i++)
		m_voice[i].m_bank = 0x8000;
	for (int i = 0; i < 3; i++)
		m_adpcm[i].m_bank = 0x8000;

	if (mode == 0)
	{
		// mode 1
		m_wet[0].m_delay = 0;
		m_dry[0].m_delay = 46;
		m_wet[1].m_delay = 0;
		m_dry[1].m_delay = 48;
		m_filter[0].m_table_pos = DATA_FILTER_TAB + (FILTER_ENTRY_SIZE*1);
		m_filter[1].m_table_pos = DATA_FILTER_TAB + (FILTER_ENTRY_SIZE*2);
		m_echo.m_end_pos = DELAY_BASE_OFFSET + 6;
		m_next_state = STATE_REFRESH1;
	}
	else
	{
		// mode 2
		m_wet[0].m_delay = 1;
		m_dry[0].m_delay = 0;
		m_wet[1].m_delay = 0;
		m_dry[1].m_delay = 0;
		m_filter[0].m_table_pos = 0xf73;
		m_filter[1].m_table_pos = 0xfa4;
		m_alt_filter[0].m_table_pos = 0xf73;
		m_alt_filter[1].m_table_pos = 0xfa4;
		m_echo.m_end_pos = DELAY_BASE_OFFSET2 + 6;
		m_next_state = STATE_REFRESH2;
	}

	m_wet[0].m_volume = 0x3fff;
	m_dry[0].m_volume = 0x3fff;
	m_wet[1].m_volume = 0x3fff;
	m_dry[1].m_volume = 0x3fff;

	m_delay_update = 1;
	m_ready_flag = 0;
	m_state_counter = 1;
}

// Updates filter parameters for mode 1
void qsound_hle_device::state_refresh_filter_1()
{
	for (int ch = 0; ch < 2; ch++)
	{
		m_filter[ch].m_delay_pos = 0;
		m_filter[ch].m_tap_count = 95;

		for (int i = 0; i < 95; i++)
			m_filter[ch].m_taps[i] = read_dsp_rom(m_filter[ch].m_table_pos + i);
	}

	m_state = m_next_state = STATE_NORMAL1;
}

// Updates filter parameters for mode 2
void qsound_hle_device::state_refresh_filter_2()
{
	for (int ch = 0; ch < 2; ch++)
	{
		m_filter[ch].m_delay_pos = 0;
		m_filter[ch].m_tap_count = 45;

		for (int i = 0; i < 45; i++)
			m_filter[ch].m_taps[i] = (int16_t)read_dsp_rom(m_filter[ch].m_table_pos + i);

		m_alt_filter[ch].m_delay_pos = 0;
		m_alt_filter[ch].m_tap_count = 44;

		for (int i = 0; i < 44; i++)
			m_alt_filter[ch].m_taps[i] = (int16_t)read_dsp_rom(m_alt_filter[ch].m_table_pos + i);
	}

	m_state = m_next_state = STATE_NORMAL2;
}

// Updates a PCM voice. There are 16 voices, each are updated every sample
// with full rate and volume control.
int16_t qsound_hle_device::qsound_voice::update(qsound_hle_device &dsp, int32_t *echo_out)
{
	// Read sample from rom and apply volume
	const int16_t output = (m_volume * dsp.read_sample(m_bank, m_addr)) >> 14;

	*echo_out += (output * m_echo) << 2;

	// Add delta to the phase and loop back if required
	int32_t new_phase = m_rate + ((m_addr << 12) | (m_phase >> 4));

	if ((new_phase >> 12) >= m_end_addr)
		new_phase -= (m_loop_len << 12);

	new_phase = std::clamp<int32_t>(new_phase, -0x8000000, 0x7FFFFFF);
	m_addr = new_phase >> 12;
	m_phase = (new_phase << 4)&0xffff;

	return output;
}

// Updates an ADPCM voice. There are 3 voices, one is updated every sample
// (effectively making the ADPCM rate 1/3 of the master sample rate), and
// volume is set when starting samples only.
// The ADPCM algorithm is supposedly similar to Yamaha ADPCM. It also seems
// like Capcom never used it, so this was not emulated in the earlier QSound
// emulators.
int16_t qsound_hle_device::qsound_adpcm::update(qsound_hle_device &dsp, int16_t curr_sample, int nibble)
{
	int8_t step;
	if (!nibble)
	{
		// Mute voice when it reaches the end address.
		if (m_cur_addr == m_end_addr)
			m_cur_vol = 0;

		// Playback start flag
		if (m_flag)
		{
			curr_sample = 0;
			m_flag = 0;
			m_step_size = 10;
			m_cur_vol = m_volume;
			m_cur_addr = m_start_addr;
		}

		// get top nibble
		step = dsp.read_sample(m_bank, m_cur_addr) >> 8;
	}
	else
	{
		// get bottom nibble
		step = dsp.read_sample(m_bank, m_cur_addr++) >> 4;
	}

	// shift with sign extend
	step >>= 4;

	// delta = (0.5 + abs(step)) * m_step_size
	int32_t delta = ((1 + abs(step << 1)) * m_step_size) >> 1;
	if (step <= 0)
		delta = -delta;
	delta += curr_sample;
	delta = std::clamp<int32_t>(delta, -32768, 32767);

	m_step_size = (dsp.read_dsp_rom(DATA_ADPCM_TAB + 8 + step) * m_step_size) >> 6;
	m_step_size = std::clamp<int16_t>(m_step_size, 1, 2000);

	return (delta * m_cur_vol) >> 16;
}

// The echo effect is pretty simple. A moving average filter is used on
// the output from the delay line to smooth samples over time.
int16_t qsound_hle_device::qsound_echo::apply(int32_t input)
{
	// get average of last 2 samples from the delay line
	int32_t old_sample = m_delay_line[m_delay_pos];
	const int32_t last_sample = m_last_sample;
	m_last_sample = old_sample;
	old_sample = (old_sample + last_sample) >> 1;

	// add current sample to the delay line
	int32_t new_sample = input + ((old_sample * m_feedback) << 2);
	m_delay_line[m_delay_pos++] = new_sample >> 16;

	if (m_delay_pos >= m_length)
		m_delay_pos = 0;

	return old_sample;
}

// Process a sample update
void qsound_hle_device::state_normal_update()
{
	m_ready_flag = 0x80;

	// recalculate echo length
	if (m_state == STATE_NORMAL2)
		m_echo.m_length = m_echo.m_end_pos - DELAY_BASE_OFFSET2;
	else
		m_echo.m_length = m_echo.m_end_pos - DELAY_BASE_OFFSET;

	m_echo.m_length = std::clamp<int16_t>(m_echo.m_length, 0, 1024);

	// update PCM voices
	int32_t echo_input = 0;
	for (int i = 0; i < 16; i++)
		m_voice_output[i] = m_voice[i].update(*this, &echo_input);

	// update ADPCM voices (one every third sample)
	const int adpcm_voice = m_state_counter % 3;
	m_voice_output[16 + adpcm_voice] = m_adpcm[adpcm_voice].update(*this, m_voice_output[16 + adpcm_voice], m_state_counter / 3);

	int16_t echo_output = m_echo.apply(echo_input);

	// now, we do the magic stuff
	for (int ch = 0; ch < 2; ch++)
	{
		// Echo is output on the unfiltered component of the left channel and
		// the filtered component of the right channel.
		int32_t wet = (ch == 1) ? echo_output << 14 : 0;
		int32_t dry = (ch == 0) ? echo_output << 14 : 0;

		for (int i = 0; i < 19; i++)
		{
			uint16_t pan_index = m_voice_pan[i] + (ch * PAN_TABLE_CH_OFFSET);

			// Apply different volume tables on the dry and wet inputs.
			dry -= (m_voice_output[i] * (int16_t)read_dsp_rom(pan_index + PAN_TABLE_DRY));
			wet -= (m_voice_output[i] * (int16_t)read_dsp_rom(pan_index + PAN_TABLE_WET));
		}
		// Saturate accumulated voices
		dry = std::clamp<int32_t>(dry, -0x1fffffff, 0x1fffffff) << 2;
		wet = std::clamp<int32_t>(wet, -0x1fffffff, 0x1fffffff) << 2;

		// Apply FIR filter on 'wet' input
		wet = m_filter[ch].apply(wet >> 16);

		// in mode 2, we do this on the 'dry' input too
		if (m_state == STATE_NORMAL2)
			dry = m_alt_filter[ch].apply(dry >> 16);

		// output goes through a delay line and attenuation
		int32_t output = (m_wet[ch].apply(wet) + m_dry[ch].apply(dry));

		// DSP round function
		output = (output + 0x2000) & ~0x3fff;
		m_out[ch] = std::clamp<int32_t>(output >> 14, -0x7fff, 0x7fff);

		if (m_delay_update)
		{
			m_wet[ch].update();
			m_dry[ch].update();
		}
	}

	m_delay_update = 0;

	// after 6 samples, the next state is executed.
	m_state_counter++;
	if (m_state_counter > 5)
	{
		m_state_counter = 0;
		m_state = m_next_state;
	}
}

// Apply the FIR filter used as the Q1 transfer function
int32_t qsound_hle_device::qsound_fir::apply(int16_t input)
{
	int32_t output = 0, tap = 0;
	for (; tap < (m_tap_count - 1); tap++)
	{
		output -= (m_taps[tap] * m_delay_line[m_delay_pos++]) << 2;

		if (m_delay_pos >= m_tap_count - 1)
			m_delay_pos = 0;
	}

	output -= (m_taps[tap] * input) << 2;

	m_delay_line[m_delay_pos++] = input;
	if (m_delay_pos >= m_tap_count - 1)
		m_delay_pos = 0;

	return output;
}

// Apply delay line and component volume
int32_t qsound_hle_device::qsound_delay::apply(const int32_t input)
{
	m_delay_line[m_write_pos++] = input >> 16;
	if (m_write_pos >= 51)
		m_write_pos = 0;

	const int32_t output = m_delay_line[m_read_pos++] * m_volume;
	if (m_read_pos >= 51)
		m_read_pos = 0;

	return output;
}

// Update the delay read position to match new delay length
void qsound_hle_device::qsound_delay::update()
{
	const int16_t new_read_pos = (m_write_pos - m_delay) % 51;
	if (new_read_pos < 0)
		m_read_pos = new_read_pos + 51;
	else
		m_read_pos = new_read_pos;
}
