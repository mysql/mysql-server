
cat <<EOF
Microsoft Developer Studio Workspace File, Format Version 6.00
# WARNING: DO NOT EDIT OR DELETE THIS WORKSPACE FILE!

###############################################################################
EOF

for i in `find . -name '*.dsp' | grep -v SCCS`
do
  name=`basename $i | sed 's/\.dsp//'`
  cat<<EOF

Project: "$name"="`echo $i | sed 's/\//\\\/g'`" - Package Owner=<4>

Package=<5>
{{{
}}}

Package=<4>
{{{
}}}

###############################################################################

EOF
done

cat<<EOF
Global:

Package=<5>
{{{
}}}

Package=<3>
{{{
}}}

###############################################################################

EOF
