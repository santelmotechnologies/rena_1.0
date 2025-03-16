Rena is a Lightweight Music Player for GNU/Linux, based on Gtk and sqlite.
It is completely written in C, based on Pragha Music Player based on Consonance
Modified and Updated to be more better.

Main features:
 * Full integration with GTK+3, but always completely independent of
   gnome or xfce.
 * Two panel design inspired by Amarok 1.4. Library and current playlist.
 * Library with multiple views, according to tags or folder structure.
 * Search, filter and queue songs on current playlist.
 * Play and edit tags of mp3, m4a, ogg, flac, asf, wma, and ape files.
   Limited only by codecs installed and taglib version used.
 * Playlist management: Export M3U and read M3U, PLS, XSPF and WAX
   playlists.
 * Playback control with command line.

Extensible by plugins:
 * AcoustID: Get metadata from AcoustID service.
 * CD-ROM: Play audio CDs and identify them on CDDB.
 * DLNA Server: Share your playlist on a DLNA server.
 * DLNA Renderer: Play music from a DLNA server.
 * Gnome-Media-Keys: Control pragha with gnome-media-keys daemon.
 * Global Hotkeys: Control pragha with multimedia keys.
 * Last.fm: Scrobbling, love, unlove song, and append similar song to get
   related playlists.
 * MPRIS2: Control pragha with mpris2 interface.
 * Mtp Devices: Basic Management of MTP devices.
 * Notification: Show a notification when changing songs.
 * Removable media: Detect removable media and scan it.
 * Song-info: Get Artist info, Lyrics and Album art of your songs.
 * Get radios: Search radios on TuneIn service.

Requirements:
 * gtk+-3.0 >= 3.8, glib-2.0 >= 2.36
 * gstreamer-1.0 >= 1.0, gstreamer-base-1.0 >= 1.0
 * taglib >= 1.8
 * sqlite3 >= 3.4

Optional:
 * libpeas-1.0 >= 1.0.0 and libpeas-gtk-1.0 >= 1.0.0: Required for all plugins.
 * libxfce4ui >= 4.11.0: Better session managament support. Save the current
   playlist, last position when saving session, etc.
 * totem-plparser >= 2.26: Support to open many more formats and internet radio
   playlists.
 * gstreamer-plugins-base-devel >= 1.0: Use cubic volume for better volume
   control.
