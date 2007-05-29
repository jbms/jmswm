
#include <wm/all.hpp>
#include <wm/extra/cwd.hpp>
#include <wm/commands.hpp>
#include <util/path.hpp>

bool update_client_visible_name_and_context(WClient *client)
{
  /* Handle xterm */
  if (client->class_name() == "XTerm")
  {
    using boost::algorithm::split_iterator;
    using boost::algorithm::make_split_iterator;
    using boost::algorithm::token_finder;
    typedef split_iterator<utf8_string::const_iterator> string_split_iterator;
    std::vector<utf8_string> parts;
    utf8_string sep = "<#!#!#>";
    for (utf8_string::size_type x = 0, next; x != utf8_string::npos; x = next)
    {
      utf8_string::size_type end = client->name().find(sep, x);
      if (end == utf8_string::npos)
      {
        next = end;
        parts.push_back(client->name().substr(x));
      }
      else
      {
        next = end + sep.size();
        parts.push_back(client->name().substr(x, end - x));
      }
    }
        
    if (parts.size() >= 2 && parts.size() <= 4)
    {
      client->set_context_info(parts[1]);
      client_cwd(client) = parts[1];

      if (parts.size() == 4)
        client->set_visible_name("[" + parts[0] + "]" + " " + parts[3]);
      else if (parts.size() == 3) 
        client->set_visible_name("[" + parts[0] + "] [" + parts[2] + "]");
      else
        client->set_visible_name("[" + parts[0] + "] xterm");
    }
    else
      client->set_visible_name(client->name());

    return true;
  }
  /* Handle emacs */
  if (client->class_name() == "Emacs")
  {
    const utf8_string &name = client->name();

    utf8_string::size_type dir_start = name.find(" [");
    if (dir_start != utf8_string::npos && name[name.size() - 1] == ']')
    {
      utf8_string context = name.substr(dir_start + 2,
                                        name.size() - dir_start - 2 - 1);
      context = compact_path_home(context);
      if (context.size() > 1 && context[context.size() - 1] == '/')
        context.resize(context.size() - 1);
      client_cwd(client) = context;
      client->set_context_info(context);
      client->set_visible_name(name.substr(0, dir_start));
    } else
    {
      client->set_visible_name(name);
      client->set_context_info(utf8_string());
      client_cwd(client).remove();
    }
    return true;
  }
  /* Handle firefox */
  if (client->class_name() == "Firefox-bin")
  {
    utf8_string suffix = " - Conkeror";
    utf8_string::size_type pos = client->name().rfind(suffix);
    if (pos + suffix.length() == client->name().length())
    {
      if (pos == 0)
        client->set_visible_name("Conkeror");
      else
        client->set_visible_name(client->name().substr(0, pos));
    } else
      client->set_visible_name(client->name());
    return true;
  }

  return false;
}
