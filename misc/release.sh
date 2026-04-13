#! /bin/bash
scp ../a.out vps:~/release/nc2000mac
scp vps:~/NC2000/a.out.exe vps:release/nc2000.exe
ssh vps 'rm -f release.zip ; cd release; zip -r ../release.zip .'
rm -f ./release.zip
scp vps:~/release.zip ./
