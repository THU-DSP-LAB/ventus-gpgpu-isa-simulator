#!/usr/bin/awk -nMf
BEGIN { flagX = 0; X = 0; }
/^[\s\S]*core[^x]+0x[0-9a-f]{16}/ {
    flagC = 0;
    for(C = 1; C < NF; C++){
        if(flagC == 0 && $C ~ /0x[0-9a-f]{16}/){
            if( flagX == 0 ) {
                X = $C;
                flagX = 1;
            }
            Y = $C - X;
            flagC = 1;
            $C = sprintf("0x%016x", Y);
        }
    }
}
{
    print;
}
