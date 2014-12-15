// Dmitry Smirnov, 2013
// Valentin Dudouyt, 2014
// 
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option) any
// later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

int Config_init(const char *conf_name);

char *Config_get_str(const char *par_name);
int Config_set_str(const char *par_name, const char *par_value);

int Config_get_int(const char *par_name);
int Config_set_int(const char *par_name, unsigned int value);

int Config_open(const char *path);
int Config_close();
