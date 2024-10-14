var saddr;
var ncores;
var nscheds;
var is_smp;

function init()
{
    Threads.clear();
    Threads.newqueue("Task List");
    Threads.setColor("Status", "waiting", "exec core 0", "blocking");

    /* Get info about the system */
    ncores = Debug.evaluate("gk_ncores");
    is_smp = Debug.evaluate("gk_is_smp");
    nscheds = is_smp ? 1 : ncores;
    saddr = new Array(nscheds);
    if(is_smp)
    {
        saddr[0] = Debug.evaluate("&sched");
        Threads.setColumns("ID", "Name", "Process", "Priority", "PC", "Status", "BlockingOn", "Stack", "Runtime (us) / %");
    }
    else
    {
        for(var i = 0; i < ncores; i++)
        {
            saddr[i] = Debug.evaluate("&scheds[" + i + "]");
        }
        Threads.setColumns("ID", "Name", "Process", "Core", "Priority", "PC", "Status", "BlockingOn", "Stack", "Runtime (us) / %");
    }
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

function task_get_process_name(t)
{
    var procaddraddr = Debug.evaluate("&((Thread *)" + t + ")->p");
    var procaddr = Debug.evaluate("*(void **)" + procaddraddr);
    var namelen = Debug.evaluate("((Process *)" + procaddr + ")->name._M_string_length");
    var nameaddr = Debug.evaluate("&((Process *)" + procaddr + ")->name._M_dataplus._M_p");
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
        var t_block_until = Debug.evaluate("((Thread *)" + t + ")->block_until._us");
        if(t_block || t_block_until != 0)
        {
            var ret = "";
            if(t_block)
            {
                switch(t_block & 0x3)
                {
                    case 0:
                        ret += task_get_name(t_block);
                        if(t == t_block)
                        {
                            ret += "( ** ERROR ** )";
                        }
                        else
                        {
                            var t_block_str = blocking_on(t_block);
                            if(t_block_str != "")
                            {
                                ret += "(" + t_block_str + ")";
                            }
                        }   
                        break;

                    case 1:
                        ret += "Signal @" + (t_block & ~3).toString(16);
                        break;

                    case 2:
                        ret += "Queue @" + (t_block & ~3).toString(16);
                        break;

                    case 3:
                        ret += "Condition @" + (t_block & ~3).toString(16);
                        break;
                }
            }
            if(t_block && t_block_until != 0)
            {
                ret += " OR ";
            }
            if(t_block_until != 0)
            {
                ret += t_block_until.toString();
                ret += " us";
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
        if(vec == undefined)
        {
            vec = Debug.evaluate("&((Scheduler *)" + csaddr + ")->tlist[" + i + "].v");
        }

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
            var pnamestr = task_get_process_name(t);

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

            if(t > 0x30000000)
            {
                t = t - 0x30000000;
            }

            if(coreid == undefined)
            {
                Threads.add(t.toString(16), namestr, pnamestr, i,
                    getregs(t)[15].toString(16), t_status,
                    blocking_on(t),
                    sp.toString(16) + " (" + stack_start.toString(16) + "-" + stack_end.toString(16) + ")",
                    time_str,
                    t);
            }
            else
            {
                Threads.add(t.toString(16), namestr, pnamestr, coreid, i,
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

    var curt = new Array(ncores);
    for(var i = 0; i < ncores; i++)
    {
        if(is_smp)
        {
            var curtaddr = Debug.evaluate("&((Scheduler *)" + saddr[0] + ")->current_thread[" + i + "]");
        }
        else
        {
            var curtaddr = Debug.evaluate("&((Scheduler *)" + saddr[i] + ")->current_thread[0]");
        }
        if(curtaddr == undefined)
        {
            curt[i] = 0;
        }
        else
        {
            curt[i] = TargetInterface.peekWord(curtaddr);
        }
    }

    var tottime = Debug.evaluate("*(uint64_t *)0x58004500") * 1000;

    if(is_smp)
    {
        add_threads_for_scheduler(saddr[0], tottime, undefined, curt, ncores);
    }
    else
    {
        for(var i = 0; i < nscheds; i++)
        {
            var core_name = i.toString();
            if(i == 0)
            {
                core_name = "M7";
            }
            else if(i == 1)
            {
                core_name = "M4";
            }
            add_threads_for_scheduler(saddr[i], tottime, core_name, curt, ncores);
        }
    }

    Threads.setColor("Status", "waiting", "exec core 0", "blocking");
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
