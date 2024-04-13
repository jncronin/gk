var saddr;
var saddr2;

function init()
{
    Threads.clear();
    Threads.newqueue("Task List");
    Threads.setColor("Status", "ready", "executing", "blocked");

    saddr = Debug.evaluate("&s");
    if(saddr == undefined)
    {
        saddr = Debug.evaluate("&sched");
        if(saddr == undefined)
        {
            saddr = Debug.evaluate("&scheds[0]");
            saddr2 = Debug.evaluate("&scheds[1]");
            Threads.setColumns("ID", "Name", "Core", "Priority", "PC", "Status", "BlockingOn", "Stack", "Runtime (ms) / %");
        }
        else
        {
            saddr2 = undefined; 
            Threads.setColumns("ID", "Name", "Priority", "PC", "Status", "BlockingOn", "Stack", "Runtime (ms) / %");
        }
    }
    else
    {
        Threads.setColumns("ID", "Name", "Priority", "PC", "Status", "BlockingOn", "Stack", "Runtime (ms) / %");
    }

    TargetInterface.message("s at " + saddr);
}

function task_get_name(t)
{
    var namelen = Debug.evaluate("((Thread *)" + t + ")->name._M_string_length");
    var nameaddr = Debug.evaluate("&((Thread *)" + t + ")->name._M_dataplus._M_p");
    var nameaddr2 = Debug.evaluate("*(uint32_t *)" + nameaddr);

    var namebytes = TargetInterface.peekBytes(nameaddr2, namelen);
    if(namebytes == undefined)
    {
        return "undefined";
    }
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
        var t_block_until = Debug.evaluate("((Thread *)" + t + ")->block_until");
        if(t_block || t_block_until != 0)
        {
            var ret = "";
            if(t_block)
            {
                ret += task_get_name(t_block);
                var t_block_str = blocking_on(t_block);
                if(t_block_str != "")
                {
                    ret += "(" + t_block_str + ")";
                }
            }
            if(t_block && t_block_until != 0)
            {
                ret += " OR ";
            }
            if(t_block_until != 0)
            {
                ret += t_block_until.toString();
                ret += " ms";
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

    for(var i = 0; i < 17; i++)
    {
        if(aRegs[i] == undefined)
        {
            aRegs[i] = 0;
        }
    }

    return aRegs;
}

function add_threads_for_scheduler(csaddr, tottime, coreid, curt, ncores)
{
    for(var i = 0; i < 5; i++)
    {
        var vec = Debug.evaluate("&((Scheduler *)" + csaddr + ")->tlist[" + i + "].v.v");

        //TargetInterface.message("vec at " + vec);

        var vstart = TargetInterface.peekWord(vec);
        var vend = TargetInterface.peekWord(vec + 4);

        //TargetInterface.message("vecs: " + vstart + " to " + vend);

        var vcur = vstart;
        while(vcur < vend)
        {
            var t = TargetInterface.peekWord(vcur);

            //TargetInterface.message("t at " + t);
            
            var namestr = task_get_name(t);

            var t_status = "waiting";
            var is_executing = false;
            for(var j = 0; j < ncores; j++)
            {
                if(t == curt[j])
                {
                    t_status = "exec core " + j;
                    is_executing = true;
                }
            }
            if(!is_executing)
            {
                if(Debug.evaluate("((Thread *)" + t + ")->for_deletion"))
                {
                    t_status = "deleted";
                }
                else if(Debug.evaluate("((Thread *)" + t + ")->is_blocking"))
                {
                    t_status = "blocking";
                }
            }
            var stack_start = Debug.evaluate("((Thread *)" + t + ")->stack.address");
            if(stack_start == undefined)
            {
                stack_start = 0;
            }
            var stack_len = Debug.evaluate("((Thread *)" + t + ")->stack.length");
            if(stack_len == undefined)
            {
                stack_len = 0;
            }
            var stack_end = stack_start + stack_len;
            var sp = getregs(t)[13];

            var ttime = Debug.evaluate("((Thread *)" + t + ")->total_us_time");
            var time_str = "";
            if(ttime != undefined)
            {
                time_str += ttime.toString(10);

                if(tottime != undefined)
                {
                    time_str += " / " + (ttime * 100.0 / tottime).toFixed(2) + "%";
                }
            }

            //TargetInterface.message("id: " + (t - 0x38000000).toString(16));
            //TargetInterface.message("name: " + namestr);
            //TargetInterface.message("i: " + i);
            //TargetInterface.message("sp: " + getregs(t)[13].toString(16));
            //TargetInterface.message("pc: " + getregs(t)[15].toString(16));
            //TargetInterface.message("status: " + t_status);
            //TargetInterface.message("blocking: " + blocking_on(t));

            if(coreid == undefined)
            {
                Threads.add((t - 0x38000000).toString(16), namestr, i,
                    getregs(t)[15].toString(16), t_status,
                    blocking_on(t),
                    sp.toString(16) + " (" + stack_start.toString(16) + "-" + stack_end.toString(16) + ")",
                    time_str,
                    t);
            }
            else
            {
                Threads.add((t - 0x38000000).toString(16), namestr, coreid, i,
                    getregs(t)[15].toString(16), t_status,
                    blocking_on(t),
                    sp.toString(16) + " (" + stack_start.toString(16) + "-" + stack_end.toString(16) + ")",
                    time_str,
                    t);
            }

            vcur += 4;
        }
    }
}

function update()
{
    Threads.clear();
    //TargetInterface.message("s at " + saddr);

    var ncores = 2;
    var curt = new Array(2);
    if(saddr2 == undefined)
    {
        for(var i = 0; i < ncores; i++)
        {
            var curtaddr = Debug.evaluate("&((Scheduler *)" + saddr + ")->current_thread[" + i + "]");
            if(curtaddr == undefined)
            {
                curt[i] = 0;
            }
            else
            {
                curt[i] = TargetInterface.peekWord(curtaddr);
            }
            //TargetInterface.message("curt[" + i + "] = " + curt[i]);
        }
    }
    else
    {
        var curtaddr1 = Debug.evaluate("&((Scheduler *)" + saddr + ")->current_thread[0]");
        curt[0] = TargetInterface.peekWord(curtaddr1);
        var curtaddr2 = Debug.evaluate("&((Scheduler *)" + saddr2 + ")->current_thread[0]");
        curt[1] = TargetInterface.peekWord(curtaddr2);
    }

    var tottime = Debug.evaluate("_cur_ms");

    if(saddr2 != undefined)
    {
        add_threads_for_scheduler(saddr, tottime, "M7", curt, ncores);
        add_threads_for_scheduler(saddr2, tottime, "M4", curt, ncores);
    }
    else
    {
        add_threads_for_scheduler(saddr, tottime, undefined, curt, ncores);
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
