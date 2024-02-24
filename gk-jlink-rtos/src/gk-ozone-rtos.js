function init()
{
    Threads.clear();
    Threads.newqueue("Task List");
    Threads.setColumns("ID", "Name", "Priority", "PC", "Status", "BlockingOn");
    Threads.setColor("Status", "ready", "executing", "blocked");
}

function task_get_name(t)
{
    var namelen = Debug.evaluate("((Thread *)" + t + ")->name._M_string_length");
    var nameaddr = Debug.evaluate("&((Thread *)" + t + ")->name._M_dataplus._M_p");
    var nameaddr2 = Debug.evaluate("*(uint32_t *)" + nameaddr);

    var namebytes = TargetInterface.peekBytes(nameaddr2, namelen);
    namestr = "";
    for(var j = 0; j < namelen; j++)
    {
        namestr += String.fromCharCode(namebytes[j]);
    }

    return namestr;
}

function blocking_on(t)
{
    if(Debug.evaluate("((Thread *)" + t + ")->is_blocking"))
    {
        var t_block = Debug.evaluate("((Thread *)" + t + ")->blocking_on");
        if(t_block)
        {
            var ret = task_get_name(t_block);
            var t_block_str = blocking_on(t_block);
            if(t_block_str != "")
            {
                ret += "(" + t_block_str + ")";
            }
            return ret;
        }
        else
        {
            return "*";
        }
    }
    else
    {
        return "";
    }
}

function getregs(t)
{
    var aRegs = new Array(17);
    for(var i = 0; i < 17; i++)
    {
        aRegs[i] = 0;
    }

    var SP = Debug.evaluate("((Thread *)" + t + ")->tss.psp");
    var addr = Debug.evaluate("&((Thread *)" + t + ")->tss.r4");

    /* Get saved values from tss */
    for(var i = 4; i < 12; i++)
    {
        aRegs[i] = TargetInterface.peekWord(addr);
        addr += 4;
    }

    var LR = TargetInterface.peekWord(addr);
    
    /* Get rest from stack */
    addr = SP;
    for(var i = 0; i < 4; i++)
    {
        aRegs[i] = TargetInterface.peekWord(addr);
        addr += 4;
    }    
    aRegs[12] = TargetInterface.peekWord(addr);
    addr += 4;
    aRegs[14] = TargetInterface.peekWord(addr);
    addr += 4;
    aRegs[15] = TargetInterface.peekWord(addr);
    addr += 4;
    aRegs[16] = TargetInterface.peekWord(addr);
    addr += 4;

    // adjust SP based upon whether FPU regs were pushed
    if((LR & 0x10) == 0)
    {
        addr += 4*18;       // s0-15, fpscr, 1x reserved word
    }

    // adjust SP based upon whether 8-byte stack alignment occured
    if(aRegs[16] & (1<<9))
    {
        addr += 4;
    }

    aRegs[13] = addr;

    return aRegs;
}

function update()
{
    Threads.clear();

    TargetInterface.message("s at " + Debug.evaluate("&s"));

    for(var i = 0; i < 10; i++)
    {
        var vec = Debug.evaluate("&s.tlist[" + i + "].v.v");
        var vstart = TargetInterface.peekWord(vec);
        var vend = TargetInterface.peekWord(vec + 4);

        var vcur = vstart;
        while(vcur < vend)
        {
            var t = TargetInterface.peekWord(vcur);
            
            var namestr = task_get_name(t);

            var t_status = "waiting";
            if(Debug.evaluate("((Thread *)" + t + ")->running_on_core"))
            {
                t_status = "executing";
            }
            else if(Debug.evaluate("((Thread *)" + t + ")->is_blocking"))
            {
                t_status = "blocking";
            }

            Threads.add((t - 0x38000000).toString(16), namestr, i,
                getregs(t)[15].toString(16), t_status,
                blocking_on(t), t);

            vcur += 4;
        }
    }
}

function getOSName()
{
    return "gkos";
}

function getContextSwitchAddrs()
{
    var aAddrs;
    var Addr;
    
    Addr = Debug.evaluate("&Yield");
    
    if (Addr != undefined) {
      aAddrs = new Array(1);
      aAddrs[0] = Addr;
      return aAddrs;
    } 
    return [];
}
