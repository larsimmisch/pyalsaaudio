from typing import list

PCM_PLAYBACK: int
PCM_CAPTURE: int

PCM_NORMAL: int
PCM_NONBLOCK: int
PCM_ASYNC: int

PCM_FORMAT_S8: int
PCM_FORMAT_U8: int
PCM_FORMAT_S16_LE: int
PCM_FORMAT_S16_BE: int
PCM_FORMAT_U16_LE: int
PCM_FORMAT_U16_BE: int
PCM_FORMAT_S24_LE: int
PCM_FORMAT_S24_BE: int
PCM_FORMAT_U24_LE: int
PCM_FORMAT_U24_BE: int
PCM_FORMAT_S32_LE: int
PCM_FORMAT_S32_BE: int
PCM_FORMAT_U32_LE: int
PCM_FORMAT_U32_BE: int
PCM_FORMAT_FLOAT_LE: int
PCM_FORMAT_FLOAT_BE: int
PCM_FORMAT_FLOAT64_LE: int
PCM_FORMAT_FLOAT64_BE: int
PCM_FORMAT_MU_LAW: int
PCM_FORMAT_A_LAW: int
PCM_FORMAT_IMA_ADPCM: int
PCM_FORMAT_MPEG: int
PCM_FORMAT_GSM: int
PCM_FORMAT_S24_3LE: int
PCM_FORMAT_S24_3BE: int
PCM_FORMAT_U24_3LE: int
PCM_FORMAT_U24_3BE: int

PCM_TSTAMP_NONE: int
PCM_TSTAMP_ENABLE: int

PCM_TSTAMP_TYPE_GETTIMEOFDAY: int
PCM_TSTAMP_TYPE_MONOTONIC: int
PCM_TSTAMP_TYPE_MONOTONIC_RAW: int

PCM_FORMAT_DSD_U8: int
PCM_FORMAT_DSD_U16_LE: int
PCM_FORMAT_DSD_U32_LE: int
PCM_FORMAT_DSD_U32_BE: int

PCM_STATE_OPEN: int
PCM_STATE_SETUP: int
PCM_STATE_PREPARED: int
PCM_STATE_RUNNING: int
PCM_STATE_XRUN: int
PCM_STATE_DRAINING: int
PCM_STATE_PAUSED: int
PCM_STATE_SUSPENDED: int
PCM_STATE_DISCONNECTED: int

MIXER_CHANNEL_ALL: int

MIXER_SCHN_UNKNOWN: int
MIXER_SCHN_FRONT_LEFT: int
MIXER_SCHN_FRONT_RIGHT: int
MIXER_SCHN_REAR_LEFT: int
MIXER_SCHN_REAR_RIGHT: int
MIXER_SCHN_FRONT_CENTER: int
MIXER_SCHN_WOOFER: int
MIXER_SCHN_SIDE_LEFT: int
MIXER_SCHN_SIDE_RIGHT: int
MIXER_SCHN_REAR_CENTER: int
MIXER_SCHN_MONO: int

VOLUME_UNITS_PERCENTAGE: int
VOLUME_UNITS_RAW: int
VOLUME_UNITS_DB: int

def pcms(pcmtype: int) -> list[str]: ...
def cards() -> list[str]: ...
def mixers(cardindex: int = -1, device: str = 'default') -> list[str]: ...
def asoundlib_version() -> str: ...

class PCM:
	def __init__(type: int = PCM_PLAYBACK, mode: int = PCM_NORMAL, rate: int = 44100, channels: int = 2,
		format: int = PCM_FORMAT_S16_LE, periodsize: int = 32, periods: int = 4,
		device: str = 'default', cardindex: int = -1) -> PCM: ...
	def close() -> None: ...
	def dumpinfo() -> None: ...
	def info() -> dict: ...
	def state() -> int: ...
	def htimestamp() -> tuple[int, int, int]: ...
	def set_tstamp_mode(mode: int = PCM_TSTAMP_ENABLE) -> None: ...
	def get_tstamp_mode() -> int: ...
	def set_tstamp_type(type: int = PCM_TSTAMP_TYPE_GETTIMEOFDAY) -> None: ...
	def get_tstamp_type() -> int: ...
	def getformats() -> dict: ...
	def getratebounds() -> tuple[int, int]: ...
	def getrates() -> int | tuple[int, int] | list[int]: ...
	def getchannels() -> list[int]: ...
	def setchannels(nchannels: int) -> None: ...
	def pcmtype() -> int: ...
	def pcmmode() -> int: ...
	def cardname() -> str: ...
	def setrate(rate: int) -> None: ...
	def setformat(format: int) -> int: ...
	def setperiodsize(period: int) -> int: ...
	def read() -> tuple[int, bytes]: ...
	def write(data: bytes) -> int: ...
	def avail() -> int: ...
	def pause(enable: bool = True) -> int: ...
	def drop() -> int: ...
	def drain() -> int: ...
	def polldescriptors() -> list[tuple[int, int]]: ...
	def polldescriptors_revents(descriptors: list[tuple[int, int]]) -> int: ...

class Mixer:
	def __init__(control: str = 'Master', id: int = 0, cardindex: int = -1, device: str = 'default') -> Mixer: ...
	def cardname() -> str: ...
	def close() -> None: ...
	def mixer() -> str: ...
	def mixerid() -> int: ...
	def switchcap() -> int: ...
	def volumecap() -> int: ...
	def getvolume(pcmtype: int = PCM_PLAYBACK, units: int = VOLUME_UNITS_PERCENTAGE) -> int: ...
	def getrange(pcmtype: int = PCM_PLAYBACK, units: int = VOLUME_UNITS_RAW) -> tuple[int, int]: ...
	def getenum() -> tuple[str, list[str]]: ...
	def getmute() -> list[int]: ...
	def getrec() -> list[int]: ...
	def setvolume(volume: int, pcmtype: int = PCM_PLAYBACK, units: int = VOLUME_UNITS_PERCENTAGE, channel: (int | None) = None) -> None: ...
	def setenum(index: int) -> None: ...
	def setmute(mute: bool, channel: (int | None) = None) -> None: ...
	def setrec(capture: int, channel: (int | None) = None) -> None: ...
	def polldescriptors() -> list[tuple[int, int]]: ...
	def handleevents() -> int: ...

