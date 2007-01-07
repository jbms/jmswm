#ifndef _UTIL_EVENT_HPP
#define _UTIL_EVENT_HPP

#include <stdlib.h>
#include <sys/types.h>
#include <event.h>
#include <boost/thread/mutex.hpp>
#include <queue>
#include <boost/function.hpp>

class EventService;

class FileEvent
{
private:
  struct event ev;
  typedef boost::function<void (short)> Handler;
  Handler handler;
  static void handle_event(int, short, void *);
  bool initialized;
public:
  FileEvent();
  FileEvent(EventService &s, int fd, short events,
            const Handler &handler);
  void initialize(EventService &s, int fd, short events,
                  const Handler &handler);
  ~FileEvent();
};

class SignalEvent
{
private:
  struct event ev;
  typedef boost::function<void ()> Handler;
  Handler handler;
  static void handle_event(int, short, void *);
  bool initialized;
public:
  SignalEvent();
  SignalEvent(EventService &s, int signal,
              const Handler &handler);
  void initialize(EventService &s, int signal,
                  const Handler &handler);
  ~SignalEvent();
};

class TimerEvent
{
private:
  struct event ev;
  typedef boost::function<void ()> Handler;
  Handler handler;
  static void handle_event(int, short, void *);
  bool initialized;
public:
  // Unlike other events, the timer is not actually started.  A wait
  // function must be used for that.
  TimerEvent(EventService &s, const Handler &handler);
  TimerEvent();
  ~TimerEvent();
  void initialize(EventService &s, const Handler &handler);

  // Cancels the current timer if it is running.
  void wait_for(long seconds,
                long microsecs);
  void cancel();
};

class EventService
{
  friend class FileEvent;
  friend class SignalEvent;
  friend class TimerEvent;
private:
  struct event_base *eb;

  // Work posting
  int post_read, post_write;
  struct event post_event;
  static void handle_post_event(int, short, void *);

  std::queue< boost::function<void ()> > work_queue;
  boost::mutex work_queue_mutex;
  
public:
  EventService();
  ~EventService();

  /* Performs a single iteration of event handling.  Blocks until at
     least one event is ready.  Never blocks after an event has been
     handled.  This must not be called by multiple threads
     concurrently. */
  int handle_events();

  /* This must not be called by the same thread that calls
     handle_events.  In a rare case, this may block until
     handle_events is called. */
  void post(const boost::function<void ()> &action);
};

#endif /* _UTIL_EVENT_HPP */
