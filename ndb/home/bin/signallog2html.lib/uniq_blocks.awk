# Copyright (C) 2004 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

BEGIN{
  NAMES[""]="";
  ORDER[0]="";
  NUM=0;
}

{
  if(NAMES[$2$3]!=$2$3){
    NAMES[$2$3]=$2$3;
    ORDER[NUM]=$2$3;
    NUM++;
  }
 
  if(NAMES[$4$5]!=$4$5){
    NAMES[$4$5]=$4$5;
    ORDER[NUM]=$4$5;
    NUM++;
  }


}
END{
  for(i=0; i<NUM; i++){
    LIST=ORDER[i]" "LIST;

  }
  print LIST;
}

