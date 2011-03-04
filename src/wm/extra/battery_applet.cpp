
#include <wm/extra/battery_applet.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>

#include <math.h>

namespace fs = boost::filesystem;

static bool read_battery_line(std::istream &is,
                              const std::string &expected_label,
                              std::string &value)
{
  std::string line;
  if (!getline(is, line))
    return false;
  
  std::string::size_type split_pos = line.find(':');
  if (split_pos == std::string::npos)
    return false;

  if (line.substr(0, split_pos) != expected_label)
    return false;

  value = boost::algorithm::trim_left_copy(line.substr(split_pos + 1));
  return true;
}

static void skip_line(std::istream &is)
{
  is.ignore(std::numeric_limits<int>::max(), '\n');
}

static bool read_battery_info(const fs::path &path,
                              int &lastFullCapacity)
{
  fs::ifstream ifs(path);
  skip_line(ifs);
  skip_line(ifs);

  std::string value;
  if (!read_battery_line(ifs, "last full capacity", value))
    return false;
  
  std::istringstream istr(value);
  if (!(istr >> lastFullCapacity))
    return false;

  return true;
}

static bool read_battery_state(const fs::path &path,
                               std::string &chargingState,
                               int &presentRate,
                               int &remainingCapacity)
{
  fs::ifstream ifs(path);

  skip_line(ifs);
  skip_line(ifs);

  if (!read_battery_line(ifs, "charging state", chargingState))
    return false;

  std::string value;

  if (!read_battery_line(ifs, "present rate", value))
    return false;

  std::istringstream istr(value);
  if (!(istr >> presentRate))
    presentRate = 0;

  if (!read_battery_line(ifs, "remaining capacity", value))
    return false;
  
  istr.clear();
  istr.str(value);
  if (!(istr >> remainingCapacity))
    remainingCapacity = 0;

  return true;
}

#if 0
int update_battery()
{

  int batteryRate = 0;
  int batteryCapacity = 0;
  int batteryMaximumCapacity = 0;
  
  int state = 0;

  if (DIR *dir = opendir("/proc/acpi/battery"))
  {
    while (dirent *d = readdir(dir)) {
      std::string name = "/proc/acpi/battery/";
      name += d->d_name;
      {
        std::string infoName = name + "/info";
        std::string stateName = name + "/state";

        int lastFullCapacity, presentRate, remainingCapacity;
        std::string chargingState;

        if (!readBatteryInfo(infoName, lastFullCapacity))
          continue;
        if (!readBatteryState(stateName, chargingState, presentRate, remainingCapacity))
          continue;

        batteryRate += presentRate;
        batteryCapacity += remainingCapacity;
        batteryMaximumCapacity += lastFullCapacity;

        if (chargingState != "charged"
            && chargingState != "unknown")
        {
          if (chargingState == "charging")
            state = 1;
          else if (chargingState == "discharging")
            state = 2;
        }
      }
      
    }
    closedir(dir);
  }

  std::ostringstream meterString;

  if ((batteryCapacity < batteryMaximumCapacity || state != 0)
      && batteryMaximumCapacity > 0)
  {
    int percent = (int)rint((double)batteryCapacity / batteryMaximumCapacity * 100.0);

    if (state == 1) {
      meterString << "\033[1;32m";
    } else if (state == 2) {
      meterString << "\033[1;34m";
    }
    meterString << "bat\033[0m ";

    meterString << std::setw(3) << percent << '%' << " ";

    if (batteryRate > 0) {
      double timeRemaining;
      if (state == 1)
        timeRemaining = (double)(batteryMaximumCapacity - batteryCapacity) / batteryRate;
      else
        timeRemaining = (double)batteryCapacity / batteryRate;

      int hoursRemaining = (int)timeRemaining;
      int minutesRemaining = (int)rint((timeRemaining - hoursRemaining) * 60.0);

      meterString << hoursRemaining << ":" << std::setfill('0') << std::setw(2) << minutesRemaining;
    }else
    {
      meterString << "    ";
    }
    
  } else {
    // "bat: [**********] 0:00"
    // "bat: 100% 0:00"
    // "              "
    
    meterString << "              ";
  }
  batteryInfoString = meterString.str();

  return 0;
}

#endif 



void BatteryApplet::event_handler()
{
  ev.wait_for(10, 0);

  int batteryRate = 0;
  int batteryCapacity = 0;
  int batteryMaximumCapacity = 0;

  enum { INACTIVE, CHARGING, DISCHARGING } state = INACTIVE;

  try
  {
    for (fs::directory_iterator it("/proc/acpi/battery"), end; it != end; ++it)
    {
      const fs::path &base = *it;

      int lastFullCapacity, presentRate, remainingCapacity;
      std::string chargingState;

      if (!read_battery_info(base / "info", lastFullCapacity))
        continue;

      if (!read_battery_state(base / "state", chargingState, presentRate,
                              remainingCapacity))
        continue;

      batteryRate += presentRate;

      if (remainingCapacity > lastFullCapacity)
        remainingCapacity = lastFullCapacity;
      
      batteryCapacity += remainingCapacity;
      batteryMaximumCapacity += lastFullCapacity;

      if (chargingState != "charged"
          && chargingState != "unknown")
      {
        if (chargingState == "charging")
          state = CHARGING;
        else if (chargingState == "discharging")
          state = DISCHARGING;
      }
    }

  } catch (std::exception &)
  {
  }

  if (batteryMaximumCapacity == 0)
  {
    // ERROR
    return;
  }

  int percent = (int)rint((double)batteryCapacity / batteryMaximumCapacity * 100.0);

  std::ostringstream ostr;
  char bat_ch;
  switch (state)
  {
  case CHARGING:
    bat_ch = '^';
    break;
  case DISCHARGING:
    bat_ch = 'v';
    break;
  default:
  case INACTIVE:
    bat_ch = '=';
    break;
  }

  ostr << bat_ch << ' ' << percent << ' ' << bat_ch;

  cell.set_text(ostr.str());

  if (state == INACTIVE)
    cell.set_style(style.inactive);
  else if (state == CHARGING)
    cell.set_style(style.charging);
  else if (state == DISCHARGING)
    cell.set_style(style.discharging);
}

BatteryApplet::BatteryApplet(WM &wm,
                             const style::Spec &style_spec,
                             WBar::InsertPosition position)
  : wm(wm),
    style(wm.dc, style_spec),
    ev(wm.event_service(), boost::bind(&BatteryApplet::event_handler, this))
{
  cell = wm.bar.insert(position, style.inactive);
  event_handler();
}
