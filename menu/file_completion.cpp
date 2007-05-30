#include <menu/list_completion.hpp>
#include <menu/file_completion.hpp>
#include <vector>
#include <util/string.hpp>
#include <util/path.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <sys/stat.h>
#include <sys/unistd.h>

namespace menu
{
  namespace file_completion
  {

    /**
       for x in $(echo $LS_COLORS | tr ':' '\n' | grep -o '\.[^=]*' | tr -d '.'); do echo "(ext_$x, menu::list_completion::EntryStyle, style::Spec),"; done > test.txt
    */

    STYLE_DEFINITION(Style,
                     ((file, menu::list_completion::EntryStyle, style::Spec),
                      (dir, menu::list_completion::EntryStyle, style::Spec),
                      (link, menu::list_completion::EntryStyle, style::Spec),
                      (fifo, menu::list_completion::EntryStyle, style::Spec),
                      (sock, menu::list_completion::EntryStyle, style::Spec),
                      (blk, menu::list_completion::EntryStyle, style::Spec),
                      (chr, menu::list_completion::EntryStyle, style::Spec),
                      (exec, menu::list_completion::EntryStyle, style::Spec),
                      (ext_cmd, menu::list_completion::EntryStyle, style::Spec),
                      (ext_exe, menu::list_completion::EntryStyle, style::Spec),
                      (ext_com, menu::list_completion::EntryStyle, style::Spec),
                      (ext_btm, menu::list_completion::EntryStyle, style::Spec),
                      (ext_bat, menu::list_completion::EntryStyle, style::Spec),
                      (ext_sh, menu::list_completion::EntryStyle, style::Spec),
                      (ext_csh, menu::list_completion::EntryStyle, style::Spec),
                      (ext_tar, menu::list_completion::EntryStyle, style::Spec),
                      (ext_tgz, menu::list_completion::EntryStyle, style::Spec),
                      (ext_arj, menu::list_completion::EntryStyle, style::Spec),
                      (ext_taz, menu::list_completion::EntryStyle, style::Spec),
                      (ext_lzh, menu::list_completion::EntryStyle, style::Spec),
                      (ext_zip, menu::list_completion::EntryStyle, style::Spec),
                      (ext_z, menu::list_completion::EntryStyle, style::Spec),
                      (ext_Z, menu::list_completion::EntryStyle, style::Spec),
                      (ext_gz, menu::list_completion::EntryStyle, style::Spec),
                      (ext_bz2, menu::list_completion::EntryStyle, style::Spec),
                      (ext_bz, menu::list_completion::EntryStyle, style::Spec),
                      (ext_tbz2, menu::list_completion::EntryStyle, style::Spec),
                      (ext_tz, menu::list_completion::EntryStyle, style::Spec),
                      (ext_deb, menu::list_completion::EntryStyle, style::Spec),
                      (ext_rpm, menu::list_completion::EntryStyle, style::Spec),
                      (ext_rar, menu::list_completion::EntryStyle, style::Spec),
                      (ext_ace, menu::list_completion::EntryStyle, style::Spec),
                      (ext_zoo, menu::list_completion::EntryStyle, style::Spec),
                      (ext_cpio, menu::list_completion::EntryStyle, style::Spec),
                      (ext_jpg, menu::list_completion::EntryStyle, style::Spec),
                      (ext_jpeg, menu::list_completion::EntryStyle, style::Spec),
                      (ext_gif, menu::list_completion::EntryStyle, style::Spec),
                      (ext_bmp, menu::list_completion::EntryStyle, style::Spec),
                      (ext_ppm, menu::list_completion::EntryStyle, style::Spec),
                      (ext_tga, menu::list_completion::EntryStyle, style::Spec),
                      (ext_xbm, menu::list_completion::EntryStyle, style::Spec),
                      (ext_xpm, menu::list_completion::EntryStyle, style::Spec),
                      (ext_tif, menu::list_completion::EntryStyle, style::Spec),
                      (ext_tiff, menu::list_completion::EntryStyle, style::Spec),
                      (ext_png, menu::list_completion::EntryStyle, style::Spec),
                      (ext_mng, menu::list_completion::EntryStyle, style::Spec),
                      (ext_xcf, menu::list_completion::EntryStyle, style::Spec),
                      (ext_pcx, menu::list_completion::EntryStyle, style::Spec),
                      (ext_mpg, menu::list_completion::EntryStyle, style::Spec),
                      (ext_mpeg, menu::list_completion::EntryStyle, style::Spec),
                      (ext_m2v, menu::list_completion::EntryStyle, style::Spec),
                      (ext_avi, menu::list_completion::EntryStyle, style::Spec),
                      (ext_mkv, menu::list_completion::EntryStyle, style::Spec),
                      (ext_ogm, menu::list_completion::EntryStyle, style::Spec),
                      (ext_mp4, menu::list_completion::EntryStyle, style::Spec),
                      (ext_m4v, menu::list_completion::EntryStyle, style::Spec),
                      (ext_mp4v, menu::list_completion::EntryStyle, style::Spec),
                      (ext_mov, menu::list_completion::EntryStyle, style::Spec),
                      (ext_qt, menu::list_completion::EntryStyle, style::Spec),
                      (ext_wmv, menu::list_completion::EntryStyle, style::Spec),
                      (ext_asf, menu::list_completion::EntryStyle, style::Spec),
                      (ext_rm, menu::list_completion::EntryStyle, style::Spec),
                      (ext_rmvb, menu::list_completion::EntryStyle, style::Spec),
                      (ext_flc, menu::list_completion::EntryStyle, style::Spec),
                      (ext_fli, menu::list_completion::EntryStyle, style::Spec),
                      (ext_gl, menu::list_completion::EntryStyle, style::Spec),
                      (ext_dl, menu::list_completion::EntryStyle, style::Spec),
                      (ext_mp3, menu::list_completion::EntryStyle, style::Spec),
                      (ext_wav, menu::list_completion::EntryStyle, style::Spec),
                      (ext_mid, menu::list_completion::EntryStyle, style::Spec),
                      (ext_midi, menu::list_completion::EntryStyle, style::Spec),
                      (ext_au, menu::list_completion::EntryStyle, style::Spec),
                      (ext_ogg, menu::list_completion::EntryStyle, style::Spec)))

    EntryStyler::EntryStyler(WDrawContext &dc, const style::Spec &style_spec)
    : style(new Style(dc, style_spec))
    {
      /*
for x in $(echo $LS_COLORS | tr ':' '\n' | grep -o '\.[^=]*' | tr -d '.'); do echo "ext_map.insert(ExtMap::value_type(\".$x\", &style->ext_$x));"; done > test.txt
      */
      ext_map.insert(ExtMap::value_type(".cmd", &style->ext_cmd));
      ext_map.insert(ExtMap::value_type(".exe", &style->ext_exe));
      ext_map.insert(ExtMap::value_type(".com", &style->ext_com));
      ext_map.insert(ExtMap::value_type(".btm", &style->ext_btm));
      ext_map.insert(ExtMap::value_type(".bat", &style->ext_bat));
      ext_map.insert(ExtMap::value_type(".sh", &style->ext_sh));
      ext_map.insert(ExtMap::value_type(".csh", &style->ext_csh));
      ext_map.insert(ExtMap::value_type(".tar", &style->ext_tar));
      ext_map.insert(ExtMap::value_type(".tgz", &style->ext_tgz));
      ext_map.insert(ExtMap::value_type(".arj", &style->ext_arj));
      ext_map.insert(ExtMap::value_type(".taz", &style->ext_taz));
      ext_map.insert(ExtMap::value_type(".lzh", &style->ext_lzh));
      ext_map.insert(ExtMap::value_type(".zip", &style->ext_zip));
      ext_map.insert(ExtMap::value_type(".z", &style->ext_z));
      ext_map.insert(ExtMap::value_type(".Z", &style->ext_Z));
      ext_map.insert(ExtMap::value_type(".gz", &style->ext_gz));
      ext_map.insert(ExtMap::value_type(".bz2", &style->ext_bz2));
      ext_map.insert(ExtMap::value_type(".bz", &style->ext_bz));
      ext_map.insert(ExtMap::value_type(".tbz2", &style->ext_tbz2));
      ext_map.insert(ExtMap::value_type(".tz", &style->ext_tz));
      ext_map.insert(ExtMap::value_type(".deb", &style->ext_deb));
      ext_map.insert(ExtMap::value_type(".rpm", &style->ext_rpm));
      ext_map.insert(ExtMap::value_type(".rar", &style->ext_rar));
      ext_map.insert(ExtMap::value_type(".ace", &style->ext_ace));
      ext_map.insert(ExtMap::value_type(".zoo", &style->ext_zoo));
      ext_map.insert(ExtMap::value_type(".cpio", &style->ext_cpio));
      ext_map.insert(ExtMap::value_type(".jpg", &style->ext_jpg));
      ext_map.insert(ExtMap::value_type(".jpeg", &style->ext_jpeg));
      ext_map.insert(ExtMap::value_type(".gif", &style->ext_gif));
      ext_map.insert(ExtMap::value_type(".bmp", &style->ext_bmp));
      ext_map.insert(ExtMap::value_type(".ppm", &style->ext_ppm));
      ext_map.insert(ExtMap::value_type(".tga", &style->ext_tga));
      ext_map.insert(ExtMap::value_type(".xbm", &style->ext_xbm));
      ext_map.insert(ExtMap::value_type(".xpm", &style->ext_xpm));
      ext_map.insert(ExtMap::value_type(".tif", &style->ext_tif));
      ext_map.insert(ExtMap::value_type(".tiff", &style->ext_tiff));
      ext_map.insert(ExtMap::value_type(".png", &style->ext_png));
      ext_map.insert(ExtMap::value_type(".mng", &style->ext_mng));
      ext_map.insert(ExtMap::value_type(".xcf", &style->ext_xcf));
      ext_map.insert(ExtMap::value_type(".pcx", &style->ext_pcx));
      ext_map.insert(ExtMap::value_type(".mpg", &style->ext_mpg));
      ext_map.insert(ExtMap::value_type(".mpeg", &style->ext_mpeg));
      ext_map.insert(ExtMap::value_type(".m2v", &style->ext_m2v));
      ext_map.insert(ExtMap::value_type(".avi", &style->ext_avi));
      ext_map.insert(ExtMap::value_type(".mkv", &style->ext_mkv));
      ext_map.insert(ExtMap::value_type(".ogm", &style->ext_ogm));
      ext_map.insert(ExtMap::value_type(".mp4", &style->ext_mp4));
      ext_map.insert(ExtMap::value_type(".m4v", &style->ext_m4v));
      ext_map.insert(ExtMap::value_type(".mp4v", &style->ext_mp4v));
      ext_map.insert(ExtMap::value_type(".mov", &style->ext_mov));
      ext_map.insert(ExtMap::value_type(".qt", &style->ext_qt));
      ext_map.insert(ExtMap::value_type(".wmv", &style->ext_wmv));
      ext_map.insert(ExtMap::value_type(".asf", &style->ext_asf));
      ext_map.insert(ExtMap::value_type(".rm", &style->ext_rm));
      ext_map.insert(ExtMap::value_type(".rmvb", &style->ext_rmvb));
      ext_map.insert(ExtMap::value_type(".flc", &style->ext_flc));
      ext_map.insert(ExtMap::value_type(".fli", &style->ext_fli));
      ext_map.insert(ExtMap::value_type(".gl", &style->ext_gl));
      ext_map.insert(ExtMap::value_type(".dl", &style->ext_dl));
      ext_map.insert(ExtMap::value_type(".mp3", &style->ext_mp3));
      ext_map.insert(ExtMap::value_type(".wav", &style->ext_wav));
      ext_map.insert(ExtMap::value_type(".mid", &style->ext_mid));
      ext_map.insert(ExtMap::value_type(".midi", &style->ext_midi));
      ext_map.insert(ExtMap::value_type(".au", &style->ext_au));
      ext_map.insert(ExtMap::value_type(".ogg", &style->ext_ogg));
    }

    EntryStyler::~EntryStyler() {}

    const menu::list_completion::EntryStyle &
    EntryStyler::operator()(const boost::filesystem::path &path,
                            struct stat const &stat_info) const
    {
      if (S_ISDIR(stat_info.st_mode))
        return style->dir;
      if ((stat_info.st_mode & S_IFMT) == S_IFLNK)
        return style->link;
      if (S_ISFIFO(stat_info.st_mode))
        return style->fifo;
      if (S_ISBLK(stat_info.st_mode))
        return style->blk;
      if (S_ISCHR(stat_info.st_mode))
        return style->chr;
      if (stat_info.st_mode & S_IXUSR)
        return style->exec;
      std::string extension = boost::filesystem::extension(path);
      ExtMap::const_iterator it = ext_map.find(extension);
      if (it != ext_map.end())
        return *it->second;
      return style->file;
    }

    static void apply_path_completion(const std::string &base,
                                      InputState &state,
                                      const utf8_string &completion)
    {
      state.text = base + completion;
      state.cursor_position = state.text.size();
    }

    static Menu::CompletionsPtr file_completions(const boost::filesystem::path &default_dir,
                                                 const InputState &state,
                                                 const EntryStyler &styler)
    {
      // No completions for the special input of ~
      if (state.text == "~")
        return Menu::CompletionsPtr();

      using boost::filesystem::path;
      using boost::filesystem::directory_iterator;
      using boost::algorithm::starts_with;
  
      std::string prefix;
      std::string base;
      {
        std::string::size_type pos = state.text.rfind('/');
        if (pos != std::string::npos)
        {
          prefix = state.text.substr(pos + 1);
          base = state.text.substr(0, pos + 1);
        }
        else
        {
          prefix = state.text;
          base = "";
        }
      }
  
      std::vector<menu::list_completion::Entry> list;
      try
      {
        for (directory_iterator it(interpret_path(default_dir, expand_path_home(base))), end;
             it != end; ++it)
        {
          const boost::filesystem::path &p = it->path();
          std::string leaf = p.leaf();
          if (starts_with(leaf, ".") && !starts_with(prefix, "."))
            continue;
          if (starts_with(leaf, prefix))
          {
            struct stat stat_info;
            if (lstat(p.string().c_str(), &stat_info) == 0)
            {
              const menu::list_completion::EntryStyle &style = styler(*it, stat_info);
              utf8_string name = leaf;
              if (S_ISDIR(stat_info.st_mode))
                name += '/';
              list.push_back(menu::list_completion::Entry(name, &style));
            }
          }
        }
      } catch (boost::filesystem::filesystem_error &e)
      {}
      return completion_list(state, list, boost::bind(&apply_path_completion,
                                                      base, _1, _2));
    }

    Menu::Completer file_completer(const boost::filesystem::path &default_dir,
                                   const EntryStyler &styler)
    {
      return boost::bind(&file_completions, default_dir, _1, boost::cref(styler));
    }
    
  } // namespace file_completion
} // namespace menu
