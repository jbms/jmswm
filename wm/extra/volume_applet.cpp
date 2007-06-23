#include <wm/extra/volume_applet.hpp>

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <sys/poll.h>
#include <stdexcept>
#include <math.h>
#include <boost/thread.hpp>


STYLE_DEFINITION(VolumeAppletStyle,
                 ((muted, WBarCellStyle, style::Spec),
                  (unmuted, WBarCellStyle, style::Spec)))


class VolumeAppletState
{
  WM &wm;
  VolumeAppletStyle style;
  WBar::CellRef cell;
  snd_mixer_t *handle;
  snd_mixer_elem_t *elem;
  snd_mixer_selem_id_t *sid;

  bool get_channel_volume(int &volume_percent,
                          bool &muted);

  void update_info(int volume_percent, bool muted);

  void update_thread_function();
public:
  VolumeAppletState(WM &wm, const style::Spec &style_spec,
                    const WBar::InsertPosition &position);
};

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


VolumeAppletState::VolumeAppletState(WM &wm, const style::Spec &style_spec,
                                     const WBar::InsertPosition &position)
  : wm(wm), style(wm.dc, style_spec)
{
  cell = wm.bar.insert(position, style.unmuted);


  try
  {
    static char card[64] = "default";

    if (snd_mixer_open(&handle, 0) < 0) {
      throw std::runtime_error("ALSAMixerInfo");
    }
    if (snd_mixer_attach(handle, card) < 0) {
      snd_mixer_close(handle);
      handle = 0;
      throw std::runtime_error("ALSAMixerInfo");
    }
    if (snd_mixer_selem_register(handle, NULL, NULL) < 0) {
      snd_mixer_close(handle);
      handle = 0;
      throw std::runtime_error("ALSAMixerInfo");
    }
    //snd_mixer_set_callback (handle, mixer_event);
    
    if (snd_mixer_load(handle) < 0) {
      snd_mixer_close(handle);
      handle = 0;
      throw std::runtime_error("ALSAMixerInfo");
    }

    // channel related
    const char *channel_name = "Master";
    sid = 0;
    snd_mixer_selem_id_malloc(&sid);
    if (!sid)
    {
      snd_mixer_close(handle);
      handle = 0;
      throw std::bad_alloc();
    }

    if (parse_simple_id(channel_name, sid) != 0)
    {
      snd_mixer_selem_id_free(sid);
      snd_mixer_close(handle);
      handle = 0;
      throw std::runtime_error("AlSAMixerInfo::Channel: invalid name");
    }
  
    elem = snd_mixer_find_selem(handle, sid);
    if (!elem)
    {
      snd_mixer_selem_id_free(sid);
      snd_mixer_close(handle);
      sid = 0;
      handle = 0;
      throw std::runtime_error("ALSAMixerInfo::Channel: handle not found");
    }

    {
      int volume_percent;
      bool muted;
      if (get_channel_volume(volume_percent, muted))
      {
        update_info(volume_percent, muted);
        boost::thread(boost::bind(&VolumeAppletState::update_thread_function, this));
      }
      else
      {
        snd_mixer_selem_id_free(sid);
        snd_mixer_close(handle);
        handle = 0;
        sid = 0;
        cell = WBar::CellRef();
      }
    }

    

  } catch (std::exception &e)
  {
    WARN("ALSA error: %s", e.what());
    cell = WBar::CellRef(); // remove bar cell
  }
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


bool VolumeAppletState::get_channel_volume(int &volume_percent, bool &muted)
{
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

void VolumeAppletState::update_thread_function()
{
  do
  {
    struct pollfd *fds;
    int count, err;
    unsigned short revents;

    /* setup for select on stdin and the mixer fd */
    if ((count = snd_mixer_poll_descriptors_count(handle)) < 0)
      break;
    fds = (pollfd*)calloc(count, sizeof(struct pollfd));
    if (fds == NULL)
      break;
    if ((err = snd_mixer_poll_descriptors(handle, fds, count)) < 0)
      break;
    if (err != count)
      break;

    int finished = poll(fds, count, -1);



    if (finished > 0)
    {
      if (snd_mixer_poll_descriptors_revents(handle, fds,
                                             count, &revents) >= 0)
      {
        if (revents & POLLNVAL)
          break;
        if (revents & POLLERR)
          break;
        if (revents & POLLIN)
          snd_mixer_handle_events(handle);
      }
    }

    {
      bool muted;
      int volume_percent;
      if (get_channel_volume(volume_percent, muted))
        wm.event_service().post(boost::bind(&VolumeAppletState::update_info, this, volume_percent, muted));
      else
        break;
    }
    
  } while (1);
  
  snd_mixer_selem_id_free(sid);
  sid = 0;
  snd_mixer_close(handle);
  handle = 0;
  cell = WBar::CellRef();
}

void VolumeAppletState::update_info(int volume_percent, bool muted)
{
  char buffer[50];
  sprintf(buffer, "vol: %d%%", volume_percent);
  cell.set_text(buffer);
  if (muted)
    cell.set_style(style.muted);
  else
    cell.set_style(style.unmuted);
}

VolumeApplet::VolumeApplet(WM &wm,
               const style::Spec &style_spec,
               const WBar::InsertPosition &position)
  : state(new VolumeAppletState(wm, style_spec, position))
{
}

VolumeApplet::~VolumeApplet()
{
}

void VolumeApplet::lower_volume()
{
  execute_shell_command("~/bin/amixer-set-Master-PCM 1-");
}

void VolumeApplet::raise_volume()
{
  execute_shell_command("~/bin/amixer-set-Master-PCM 1+");
}

void VolumeApplet::toggle_mute()
{
  execute_shell_command("amixer set Master toggle");
}
