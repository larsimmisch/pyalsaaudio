from _typeshed import ReadableBuffer
from typing import Final, Literal, TypedDict, final

PCM_PLAYBACK: Final[int]
PCM_CAPTURE: Final[int]

PCM_NORMAL: Final[int]
PCM_NONBLOCK: Final[int]
PCM_ASYNC: Final[int]

PCM_FORMAT_S8: Final[int]
PCM_FORMAT_U8: Final[int]
PCM_FORMAT_S16_LE: Final[int]
PCM_FORMAT_S16_BE: Final[int]
PCM_FORMAT_U16_LE: Final[int]
PCM_FORMAT_U16_BE: Final[int]
PCM_FORMAT_S24_LE: Final[int]
PCM_FORMAT_S24_BE: Final[int]
PCM_FORMAT_U24_LE: Final[int]
PCM_FORMAT_U24_BE: Final[int]
PCM_FORMAT_S32_LE: Final[int]
PCM_FORMAT_S32_BE: Final[int]
PCM_FORMAT_U32_LE: Final[int]
PCM_FORMAT_U32_BE: Final[int]
PCM_FORMAT_FLOAT_LE: Final[int]
PCM_FORMAT_FLOAT_BE: Final[int]
PCM_FORMAT_FLOAT64_LE: Final[int]
PCM_FORMAT_FLOAT64_BE: Final[int]
PCM_FORMAT_MU_LAW: Final[int]
PCM_FORMAT_A_LAW: Final[int]
PCM_FORMAT_IMA_ADPCM: Final[int]
PCM_FORMAT_MPEG: Final[int]
PCM_FORMAT_GSM: Final[int]
PCM_FORMAT_S24_3LE: Final[int]
PCM_FORMAT_S24_3BE: Final[int]
PCM_FORMAT_U24_3LE: Final[int]
PCM_FORMAT_U24_3BE: Final[int]

PCM_TSTAMP_NONE: Final[int]
PCM_TSTAMP_ENABLE: Final[int]

PCM_TSTAMP_TYPE_GETTIMEOFDAY: Final[int]
PCM_TSTAMP_TYPE_MONOTONIC: Final[int]
PCM_TSTAMP_TYPE_MONOTONIC_RAW: Final[int]

PCM_FORMAT_DSD_U8: Final[int]
PCM_FORMAT_DSD_U16_LE: Final[int]
PCM_FORMAT_DSD_U32_LE: Final[int]
PCM_FORMAT_DSD_U32_BE: Final[int]

PCM_STATE_OPEN: Final[int]
PCM_STATE_SETUP: Final[int]
PCM_STATE_PREPARED: Final[int]
PCM_STATE_RUNNING: Final[int]
PCM_STATE_XRUN: Final[int]
PCM_STATE_DRAINING: Final[int]
PCM_STATE_PAUSED: Final[int]
PCM_STATE_SUSPENDED: Final[int]
PCM_STATE_DISCONNECTED: Final[int]

MIXER_CHANNEL_ALL: Final[int]

# NOTE: Omit for now - use case unknown (see alsaaudio.c)
#MIXER_SCHN_UNKNOWN: Final[int]
#MIXER_SCHN_FRONT_LEFT: Final[int]
#MIXER_SCHN_FRONT_RIGHT: Final[int]
#MIXER_SCHN_REAR_LEFT: Final[int]
#MIXER_SCHN_REAR_RIGHT: Final[int]
#MIXER_SCHN_FRONT_CENTER: Final[int]
#MIXER_SCHN_WOOFER: Final[int]
#MIXER_SCHN_SIDE_LEFT: Final[int]
#MIXER_SCHN_SIDE_RIGHT: Final[int]
#MIXER_SCHN_REAR_CENTER: Final[int]
#MIXER_SCHN_MONO: Final[int]

VOLUME_UNITS_PERCENTAGE: Final[int]
VOLUME_UNITS_RAW: Final[int]
VOLUME_UNITS_DB: Final[int]

_PCMType = int | Literal["playback", "capture"] | None

_PCMInfo = TypedDict(
    "_PCMInfo",
    {
        "name": str,
        "card_no": int,
        "device_no": int,
        "subdevice_no": int,
        "state": str,
        "access_type": str,
        " (call value) type": int,
        " (call value) type_name": str,
        " (call value) mode": int,
        " (call value) mode_name": str,
        "format": int,
        "format_name": str,
        "format_description": str,
        "subformat_name": str,
        "subformat_description": str,
        "channels": int,
        "rate": int,
        "period_time": int,
        "period_size": int,
        "buffer_time": int,
        "buffer_size": int,
        "periods": int,
        "rate_numden": tuple[int, int],
        "significant_bits": int,
        "nominal_bits": int,
        "physical_bits": int,
        "is_batch": bool,
        "is_block_transfer": bool,
        "is_double": bool,
        "is_half_duplex": bool,
        "is_joint_duplex": bool,
        "can_overrange": bool,
        "can_mmap_sample_resolution": bool,
        "can_pause": bool,
        "can_resume": bool,
        "can_sync_start": bool,
    },
)

def pcms(pcmtype: _PCMType = PCM_PLAYBACK) -> list[str]: ...
def cards() -> list[str]: ...
def mixers(cardindex: int = -1, device: str = 'default') -> list[str]: ...
def asoundlib_version() -> str: ...

def card_indexes() -> list[int]: ...
def card_name(index: int) -> tuple[str, str]: ...


@final
class PCM:
	def __new__(
		cls,
		type: _PCMType = PCM_PLAYBACK,
		mode: int = PCM_NORMAL,
		device: str = "default",
		cardindex: int = -1,
		card: str | None = None,
		rate: int = 44100,
		channels: int = 2,
		format: int = PCM_FORMAT_S16_LE,
		periodsize: int = 32,
		periods: int = 4,
	) -> PCM: ...
	def close(self) -> None: ...
	def dumpinfo(self) -> None: ...
	def info(self) -> _PCMInfo: ...
	def state(self) -> int: ...
	def htimestamp(self) -> tuple[int, int, int]: ...
	def set_tstamp_mode(self, mode: int = PCM_TSTAMP_ENABLE) -> None: ...
	def get_tstamp_mode(self) -> int: ...
	def set_tstamp_type(self, type: int = PCM_TSTAMP_TYPE_GETTIMEOFDAY) -> None: ...
	def get_tstamp_type(self) -> int: ...
	def getformats(self) -> dict[str, int]: ...
	def getratebounds(self) -> tuple[int, int]: ...
	def getrates(self) -> int | tuple[int, int] | list[int]: ...
	def getchannels(self) -> list[int]: ...
	def setchannels(self, nchannels: int) -> int: ...
	def pcmtype(self) -> int: ...
	def pcmmode(self) -> int: ...
	def cardname(self) -> str: ...
	def setrate(self, rate: int) -> int: ...
	def setformat(self, format: int) -> int: ...
	def setperiodsize(self, period: int) -> int: ...
	def read(self) -> tuple[int, bytes]: ...
	def write(self, data: ReadableBuffer) -> int: ...
	def avail(self) -> int: ...
	def pause(self, enable: bool = True) -> int: ...
	def drop(self) -> int: ...
	def drain(self) -> int: ...
	def polldescriptors(self) -> list[tuple[int, int]]: ...
	def polldescriptors_revents(self, descriptors: list[tuple[int, int]]) -> int: ...

@final
class Mixer:
	def __new__(cls, control: str = 'Master', id: int = 0, cardindex: int = -1, device: str = 'default') -> Mixer: ...
	def cardname(self) -> str: ...
	def close(self) -> None: ...
	def mixer(self) -> str: ...
	def mixerid(self) -> int: ...
	def switchcap(self) -> list[str]: ...
	def volumecap(self) -> list[str]: ...
	def getvolume(self, pcmtype: _PCMType = None, units: int = VOLUME_UNITS_PERCENTAGE) -> list[int]: ...
	def getrange(self, pcmtype: _PCMType = None, units: int = VOLUME_UNITS_RAW) -> list[int]: ...
	def getenum(self) -> tuple[()] | tuple[str, list[str]]: ...
	def getmute(self) -> list[int]: ...
	def getrec(self) -> list[int]: ...
	def setvolume(self, volume: int, channel: int = MIXER_CHANNEL_ALL, pcmtype: _PCMType = None, units: int = VOLUME_UNITS_PERCENTAGE) -> None: ...
	def setenum(self, index: int) -> None: ...
	def setmute(self, mute: int, channel: int = MIXER_CHANNEL_ALL) -> None: ...
	def setrec(self, capture: int, channel: int = MIXER_CHANNEL_ALL) -> None: ...
	def polldescriptors(self) -> list[tuple[int, int]]: ...
	def handleevents(self) -> int: ...

class ALSAAudioError(Exception): ...
