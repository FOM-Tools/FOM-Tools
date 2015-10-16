BEGIN{
    if(pbegin){
	print "start page is ",pbegin;
	addr=strtonum(pbegin);
    }else{
	addr=0x5488f000;
    }

    if(pend){
	print "end page is ",pend;
	addend=strtonum(pend);
    }else{
	addend=0x5742e000;
    }
    #addend= addr + (1192*4096) ;
    printf("searching for %x to %x\n",addr,addend);
}
{
    rb=strtonum($2);
    re=strtonum($3);
    #printf("%x,%x\n", rb, re);
    if(rb<=addr){
	if(re>=addr){#beginning is inside allocation
	    if(re>=addend){#allocation covers range 
		printf("Superset Overlap\t0x%x-0x%x ,%s\n",rb,
		       re,$0);
	    }else{#allocation underflows range
		printf("Uflow Overlap\t0x%x-0x%x , %s\n",rb,re,$0);
	    }
	}#else allocation is below range
    }else{
	if(rb<=addend){#allocation starts inside range
	    if(re<=addend){#allocation is inside range
		printf("Subset Overlap\t0x%x-0x%x , %s\n",rb,re,$0);
	    }else{# allocation overflows range
		printf("Oflow Overlap\t0x%x-0x%x , %s\n",rb,re,$0);
	    }
	}#else allocation is above range
    }
}
