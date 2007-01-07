#ifndef _WM_VOLUME_HPP
#define _WM_VOLUME_HPP

#include <alsa/asoundlib.h>
#include <boost/thread/thread.hpp>

class ALSAMixerInfo
{
  snd_mixer_t *handle;
public:

  class Channel
  {
  private:
    ALSAMixerInfo &info;
	snd_mixer_elem_t *elem;

    friend class ALSAMixerInfo;

  public:
    Channel(ALSAMixerInfo &info, const char *channel_name);
  };

  ALSAMixerInfo();
  ~ALSAMixerInfo();

  bool get_channel_volume(Channel &channel,
                          int &volume_percent,
                          bool &muted);

  

  
};

#endif /* _WM_VOLUME_HPP */
