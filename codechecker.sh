#!/bin/sh
/snap/bin/codechecker analyze compile_commands.json -o ./reports
/snap/bin/codechecker parse ./reports -e html -o ./reports_html
return 0
#/snap/bin/firefox ./reports_html/index.html