var ncores;
var nprio;
var saddr;

function init()
{
    Threads.clear();
    Threads.newqueue("Task List");
    Threads.setColor("Status", "waiting", "exec core 0", "blocking");

    ncores = Debug.evaluate("gk_ncores");
    nprio = Debug.evaluate("gk_nprio");
    saddr = Debug.evaluate("&sched");

    if(ncores == undefined)
    {
        ncores = 2;
    }

    if(nprio == undefined)
    {
        nprio = 5;
    }

    TargetInterface.message("sched at " + saddr.toString(16));
    TargetInterface.message("ncores = " + ncores);
    TargetInterface.message("nprio = " + nprio);


    Threads.setColumns("ID", "Name", "Process", "Priority", "PC", "Status", "BlockingOn", "Stack", "Runtime (us) / %");
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
    if(namelen == 0)
    {
        return "undefined";
    }
    var nameaddr = Debug.evaluate("&((Process *)" + procaddr + ")->name._M_dataplus._M_p");
    var nameaddr2 = Debug.evaluate("*(void **)" + nameaddr);
    if(nameaddr2 == 0)
    {
        return "undefined";
    }

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
        var t_blocking_on_thread = Debug.evaluate("((Thread *)" + t + ")->blocking_on_thread");
        var t_blocking_on_prim = Debug.evaluate("((Thread *)" + t + ")->blocking_on_prim");
        var t_block_until_ns = Debug.evaluate("((Thread *)" + t + ")->block_until.tv_nsec");
        var t_block_until_s = Debug.evaluate("((Thread *)" + t + ")->block_until.tv_sec");

        var ret = "";

        if(t_blocking_on_thread)
        {
            if(ret != "")
            {
                ret = ret + ", ";
            }
            ret = ret + task_get_name(t_blocking_on_thread);
        }
        if(t_blocking_on_prim)
        {
            if(ret != "")
            {
                ret = ret + ", ";
            }
            ret = ret + "sync @" + t_blocking_on_prim.toString(16);
        }
        if(t_block_until_ns || t_block_until_s)
        {
            if(ret != "")
            {
                ret = ret + t_block_until_s.toString() + "." + t_block_until_ns.toString();
            }
        }
        return ret;
    }

    return "";
}

function getregs(t)
{
    var aRegs = new Array(42);
    for(var i = 0; i < 42; i++)
    {
        aRegs[i] = 0;
    }

    var SP = Debug.evaluate("((Thread *)" + t + ")->tss.sp_el1");

    /* get values from tss */
    var addr = Debug.evaluate("&((Thread *)" + t + ")->tss.r19");

    /* Get saved values from tss */
    for(var i = 19; i < 29; i++)
    {
        aRegs[i] = TargetInterface.peekWord(addr);
        addr += 8;
    }

    /* Get rest from stack */
    addr = SP;
    for(var i = 0; i < 19; i++)
    {
        aRegs[i] = TargetInterface.peekWord(addr);
        addr += 8;
    }

    aRegs[41] = TargetInterface.peekWord(addr);
    addr += 8;

    aRegs[33] = TargetInterface.peekWord(addr);
    addr += 8;

    aRegs[32] = SP;
    aRegs[29] = TargetInterface.peekWord(SP + 320);
    aRegs[30] = TargetInterface.peekWord(SP + 328);
}

function add_threads_for_scheduler()
{
    TargetInterface.message("saddr = " + saddr.toString(16));
    var curt = new Array(ncores);
    for(var i = 0; i < ncores; i++)
    {
        var curtaddrstr = "&((Scheduler *)0x" + saddr.toString(16) + ")->current_thread[" + i + "]";
        var curtaddr = Debug.evaluate(curtaddrstr);
        TargetInterface.message("curtaddr = " + curtaddr.toString(16));
        curt[i] = Debug.evaluate("*(void **)(&((Scheduler *)" + saddr + ")->current_thread[" + i + "])");
        TargetInterface.message("curt[" + i + "] = " + curt[i].toString(16));
    }
    for(var i = 0; i < nprio; i++)
    {
        vec = Debug.evaluate("&((Scheduler *)" + saddr + ")->tlist[" + i + "].v");

        //TargetInterface.message("vec at " + vec);

        var vstart = TargetInterface.peekWord(vec);
        var vend = TargetInterface.peekWord(vec + 8);

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

/*
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
*/

/*
            var ttime = Debug.evaluate("((Thread *)" + t + ")->total_us_time");
            var time_str = "";
            if(ttime != undefined)
            {
                time_str += ttime.toString(10);

                if(tottime != undefined)
                {
                    time_str += " / " + (ttime * 100.0 / (tottime - last_tottime)).toFixed(2) + "%";
                    var tustime_addr = Debug.evaluate("&((Thread *)" + t + ")->total_us_time");
                    if(tustime_addr != undefined)
                    {
                        TargetInterface.pokeWord(tustime_addr, 0);
                    }
                    else
                    {
                        TargetInterface.message("cant get address");
                    }
                }
            }
*/

            //TargetInterface.message("id: " + (t - 0x38000000).toString(16));
            //TargetInterface.message("name: " + namestr);
            //TargetInterface.message("i: " + i);
            //TargetInterface.message("sp: " + getregs(t)[13].toString(16));
            //TargetInterface.message("pc: " + getregs(t)[15].toString(16));
            //TargetInterface.message("status: " + t_status);
            //TargetInterface.message("blocking: " + blocking_on(t));

            Threads.add(t.toString(16), namestr, pnamestr, i,
                getregs(t)[33].toString(16), t_status,
                blocking_on(t),
                getregs(t)[32].toString(16),
                "",
                t);

            vcur += 8;
        }
    }
}

function update()
{
    Threads.clear();

    //var tottime = Debug.evaluate("*(uint64_t *)0x58004500") * 1000;

    add_threads_for_scheduler(saddr[0], tottime, undefined, curt, ncores);

/*
    if(tottime != undefined)
    {
        last_tottime = tottime;
    } */

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
