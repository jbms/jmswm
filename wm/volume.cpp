#include <wm/volume.hpp>
#include <ctype.h>
#include <math.h>
#include <stdexcept>

static int parse_simple_id(const char *str, snd_mixer_selem_id_t *sid)
{
	int c, size;
	char buf[128];
	char *ptr = buf;

	while (*str == ' ' || *str == '\t')
		str++;
	if (!(*str))
		return -EINVAL;
	size = 1;	/* for '\0' */
	if (*str != '"' && *str != '\'') {
		while (*str && *str != ',') {
			if (size < (int)sizeof(buf)) {
				*ptr++ = *str;
				size++;
			}
			str++;
		}
	} else {
		c = *str++;
		while (*str && *str != c) {
			if (size < (int)sizeof(buf)) {
				*ptr++ = *str;
				size++;
			}
			str++;
		}
		if (*str == c)
			str++;
	}
	if (*str == '\0') {
		snd_mixer_selem_id_set_index(sid, 0);
		*ptr = 0;
		goto _set;
	}
	if (*str != ',')
		return -EINVAL;
	*ptr = 0;	/* terminate the string */
	str++;
	if (!isdigit(*str))
		return -EINVAL;
	snd_mixer_selem_id_set_index(sid, atoi(str));
       _set:
	snd_mixer_selem_id_set_name(sid, buf);
	return 0;
}

static int convert_prange(int val, int min, int max)
{
	int range = max - min;
	int tmp;

	if (range == 0)
		return 0;
	val -= min;
	tmp = (int)rint((double)val/(double)range * 100);
	return tmp;
}

ALSAMixerInfo::ALSAMixerInfo()
{
  static char card[64] = "default";

  if (snd_mixer_open(&handle, 0) < 0) {
    throw std::runtime_error("ALSAMixerInfo");
  }
  if (snd_mixer_attach(handle, card) < 0) {
    snd_mixer_close(handle);
    throw std::runtime_error("ALSAMixerInfo");
  }
  if (snd_mixer_selem_register(handle, NULL, NULL) < 0) {
    snd_mixer_close(handle);
    throw std::runtime_error("ALSAMixerInfo");
  }
  if (snd_mixer_load(handle) < 0) {
    snd_mixer_close(handle);
    throw std::runtime_error("ALSAMixerInfo");
  }
}

ALSAMixerInfo::~ALSAMixerInfo()
{
  snd_mixer_close(handle);
}


ALSAMixerInfo::Channel::Channel(ALSAMixerInfo &info, const char *channel_name)
  : info(info)
{
  snd_mixer_selem_id_t *sid;
  snd_mixer_selem_id_alloca(&sid);
  
  if (parse_simple_id(channel_name, sid) != 0)
    throw std::runtime_error("AlSAMixerInfo::Channel: invalid name");
  
  elem = snd_mixer_find_selem(info.handle, sid);
  if (!elem)
    throw std::runtime_error("ALSAMixerInfo::Channel: handle not found");
}

bool ALSAMixerInfo::get_channel_volume(Channel &channel,
                                       int &volume_percent,
                                       bool &muted)
{
  snd_mixer_elem_t *elem = channel.elem;
  snd_mixer_selem_channel_id_t chn;
  
  int pmono;
  long pmin = 0, pmax = 0, pvol = 0;
  int psw = 1;
  bool total_found = false;

  muted = false;
  volume_percent = 0;

  if (snd_mixer_selem_has_common_volume(elem)
      || snd_mixer_selem_has_playback_volume(elem)) {
    snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
  }

  pmono = snd_mixer_selem_has_playback_channel(elem, SND_MIXER_SCHN_MONO) &&
    (snd_mixer_selem_is_playback_mono(elem) || 
     (!snd_mixer_selem_has_playback_volume(elem) &&
      !snd_mixer_selem_has_playback_switch(elem)));
    
  for (chn = (snd_mixer_selem_channel_id_t)0;
       chn <= SND_MIXER_SCHN_LAST;
       chn = snd_mixer_selem_channel_id_t((int)chn + 1)) {
    bool found = false;
    if (pmono || !snd_mixer_selem_has_playback_channel(elem, chn))
      continue;
    if (!snd_mixer_selem_has_common_volume(elem)) {
      if (snd_mixer_selem_has_playback_volume(elem)) {
        snd_mixer_selem_get_playback_volume(elem, chn, &pvol);
        found = true;
        total_found = true;
      }
    }
    if (!snd_mixer_selem_has_common_switch(elem)) {
      if (snd_mixer_selem_has_playback_switch(elem)) {
        snd_mixer_selem_get_playback_switch(elem, chn, &psw);
        total_found = true;
      }
    }
  }

  if (!total_found)
    return false;

  muted = psw ? false : true;
  volume_percent = convert_prange(pvol, pmin, pmax);

  return true;
}
