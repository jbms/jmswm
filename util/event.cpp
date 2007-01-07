
#include <util/event.hpp>
#include <util/log.hpp>
#include <assert.h>

EventService::EventService()
{
  eb = (event_base *)event_init();

  int filedes[2];
  if (pipe(filedes) != 0)
    ERROR_SYS("pipe");

  post_read = filedes[0];
  post_write = filedes[1];

  event_set(&post_event, post_read, EV_READ | EV_PERSIST,
            &EventService::handle_post_event, this);
  if (event_base_set(eb, &post_event) != 0)
    ERROR_SYS("event_base_set");
  if (event_add(&post_event, 0) != 0)
    ERROR_SYS("event_add");
}

EventService::~EventService()
{
  close(post_read);
  close(post_write);
  event_del(&post_event);
}

void EventService::handle_post_event(int, short, void *ptr)
{
  EventService *s = (EventService *)ptr;
  const int buffer_size = 128;
  char buffer[buffer_size];
  while (1)
  {
    int result = read(s->post_read, buffer, buffer_size);
    if (result == EINTR)
      continue;
    if (result == 0)
      ERROR("post read EOF received");
    if (result < 0)
      ERROR_SYS("post read");
    break;
  }
  
  boost::mutex::scoped_lock l(s->work_queue_mutex);
  while (!s->work_queue.empty())
  {
    s->work_queue.front()();
    s->work_queue.pop();
  }
}

void EventService::post(const boost::function<void ()> &action)
{
  {
    boost::mutex::scoped_lock l(work_queue_mutex);
    work_queue.push(action);
  }

  char c = 0;
  while (1)
  {
    int result = write(post_write, &c, 1);
    if (result == EINTR)
      continue;
    if (result == 0)
      continue;
    if (result < 0)
      ERROR_SYS("post write");
    break;
  }
}

int EventService::handle_events()
{
  return event_base_loop(eb, EVLOOP_ONCE);
}

FileEvent::FileEvent()
  : initialized(false)
{
}

FileEvent::FileEvent(EventService &s, int fd, short events,
                     const Handler &handler)
  : initialized(false)
{
  initialize(s, fd, events, handler);
}

void FileEvent::initialize(EventService &s, int fd, short events,
                           const Handler &handler)
{
  assert(initialized == false);
  initialized = true;
  this->handler = handler;
  event_set(&ev, fd, events | EV_PERSIST,
            &FileEvent::handle_event, this);
  if (event_base_set(s.eb, &ev) != 0)
    ERROR_SYS("event_base_set");
  if (event_add(&ev, 0) != 0)
    ERROR_SYS("event_add");
}

void FileEvent::handle_event(int, short events, void *ptr)
{
  FileEvent *e = (FileEvent *)ptr;
  e->handler(events);
}

FileEvent::~FileEvent()
{
  if (initialized)
    event_del(&ev);
}

SignalEvent::SignalEvent()
  : initialized(false)
{}

SignalEvent::SignalEvent(EventService &s, int signal,
                         const Handler &handler)
  : initialized(false)
{
  initialize(s, signal, handler);
}


void SignalEvent::initialize(EventService &s, int signal,
                             const Handler &handler)
{
  assert(initialized == false);
  initialized = true;
  this->handler = handler;
  signal_set(&ev, signal, &SignalEvent::handle_event, this);
  if (event_base_set(s.eb, &ev) != 0)
    ERROR_SYS("event_base_set");
  if (event_add(&ev, 0) != 0)
    ERROR_SYS("event_add");
}

void SignalEvent::handle_event(int, short, void *ptr)
{
  SignalEvent *e = (SignalEvent *)ptr;
  e->handler();
}

SignalEvent::~SignalEvent()
{
  if (initialized)
    event_del(&ev);
}

TimerEvent::TimerEvent()
  : initialized(false)
{}

TimerEvent::TimerEvent(EventService &s,
                       const Handler &handler)
  : initialized(false)
{
  initialize(s, handler);
}


void TimerEvent::initialize(EventService &s,
                            const Handler &handler)
{
  assert(initialized == false);
  initialized = true;
  this->handler = handler;
  evtimer_set(&ev, &TimerEvent::handle_event, this);
  if (event_base_set(s.eb, &ev) != 0)
    ERROR_SYS("event_base_set");
}

void TimerEvent::handle_event(int, short, void *ptr)
{
  TimerEvent *e = (TimerEvent *)ptr;
  e->handler();
}

TimerEvent::~TimerEvent()
{
  if (initialized)
    event_del(&ev);
}

void TimerEvent::wait_for(long seconds, long microsecs)
{
  struct timeval tv;
  tv.tv_sec = seconds;
  tv.tv_usec = microsecs;
  
  if (event_add(&ev, &tv) != 0)
    ERROR_SYS("event_add");
}

void TimerEvent::cancel()
{
  event_del(&ev);
}
