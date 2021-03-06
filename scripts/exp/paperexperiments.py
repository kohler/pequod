# list of experiments exported to calling script
exps = []

# add experiments to the global list.
# a local function that keeps the internal variables from becoming global
def define_experiments():
    binary = True
    binaryflag = "" if binary else "--no-binary"
    partfunc = "twitternew" if binary else "twitternew-text"
    fetch = "--fetch"
#     fetch = ""
    
    serverCmd = "./obj/pqserver"
    appCmd = "./obj/pqserver --twitternew --verbose"
    initCmd = "%s %s --initialize --no-populate --no-execute" % (appCmd, binaryflag)
    clientCmd = "%s %s %s --no-initialize --popduration=0" % (appCmd, binaryflag, fetch)

    # policy experiment
    # can be run on on a multiprocessor
    #
    # we set the number of operations so that there are 
    # about 56 timeline checks per active user.
    # the number of posts is fixed, so there ends up being a variable
    # post:check ratio of 1:1 at 1% active users to 1:100 at 100% active users
    exp = {'name': "policy", 'defs': []}
    users = "--graph=twitter_graph_1.8M.dat"
    
    points = [1, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100]
    for active in points:
        clientBase = "%s %s --pactive=%d --duration=1000000000 --postlimit=1000000 " \
                     "--ppost=1 --pread=%d --psubscribe=0 --plogout=0" % \
                     (clientCmd, users, active, active)
        
        exp['defs'].append(
            {'name': "hybrid_%d" % (active),
             'def_part': partfunc,
             'backendcmd': "%s" % (serverCmd),
             'cachecmd': "%s" % (serverCmd),
             'initcmd': "%s" % (initCmd),
             'clientcmd': "%s --no-prevalidate" % (clientBase)})
    
        exp['defs'].append(
            {'name': "pull_%d" % (active),
             'def_part': partfunc,
             'backendcmd': "%s" % (serverCmd),
             'cachecmd': "%s" % (serverCmd),
             'initcmd': "%s --pull" % (initCmd),
             'clientcmd': "%s --pull" % (clientBase)})
        
        exp['defs'].append(
            {'name': "push_%d" % (active),
             'def_part': partfunc,
             'backendcmd': "%s" % (serverCmd),
             'cachecmd': "%s" % (serverCmd),
             'initcmd': "%s" % (initCmd),
             'clientcmd': "%s --prevalidate --prevalidate-inactive" % (clientBase)})
    
    exp['plot'] = {'type': "line",
                   'data': [{'from': "client",
                             'attr': "wall_time"}],
                   'lines': ["hybrid", "pull", "push"],
                   'points': points,
                   'xlabel': "Percent Active",
                   'ylabel': "Runtime (s)"}
    exps.append(exp)
    

    # client push vs. pequod experiment
    # can be run on a multiprocessor
    #
    # fix the %active at 70 and have each user perform 50 timeline checks.
    # fix the post:check ratio at 1:100 
    exp = {'name': "client_push", 'defs': []}
    users = "--graph=twitter_graph_1.8M.dat"
    clientBase = "%s %s --pactive=70 --duration=1000000000 --checklimit=62795845 " \
                 "--ppost=1 --pread=100 --psubscribe=10 --plogout=5" % \
                 (clientCmd, users)
    
    exp['defs'].append(
        {'name': "pequod",
         'def_part': partfunc,
         'backendcmd': "%s" % (serverCmd),
         'cachecmd': "%s" % (serverCmd),
         'initcmd': "%s" % (initCmd),
         'clientcmd': "%s --no-prevalidate" % (clientBase)})
    
    exp['defs'].append(
        {'name': "pequod-warm",
         'def_part': partfunc,
         'backendcmd': "%s" % (serverCmd),
         'cachecmd': "%s" % (serverCmd),
         'initcmd': "%s" % (initCmd),
         'clientcmd': "%s --prevalidate" % (clientBase)})
    
    exp['defs'].append(
        {'name': "client-push",
         'def_part': partfunc,
         'backendcmd': "%s" % (serverCmd),
         'cachecmd': "%s" % (serverCmd),
         'initcmd': "%s --push" % (initCmd),
         'clientcmd': "%s --push" % (clientBase)})
    
    exp['plot'] = {'type': "stackedbar",
                   'data': [{'from': "server",
                             'attr': "server_wall_time_insert"},
                            {'from': "server",
                             'attr': "server_wall_time_validate"},
                            {'from': "server",
                             'attr': "server_wall_time_other"}],
                   'lines': ["pequod", "pequod-warm", "client-push"],
                   'ylabel': "Runtime (s)"}
    exps.append(exp)


    # pequod optimization factor analysis experiment.
    # this test will need to be run multiple times against different builds.
    # specifically, the code needs to be reconfigured with --disable-hint and 
    # --disable-value-sharing to produce the results for the different factors.
    # can be run on a multiprocessor
    exp = {'name': "optimization", 'defs': []}
    users = "--graph=twitter_graph_1.8M.dat"
    clientBase = "%s %s --pactive=70 --duration=1000000000 --checklimit=62795845 " \
                 "--ppost=1 --pread=100 --psubscribe=10 --plogout=5" % \
                 (clientCmd, users)
    
    exp['defs'].append(
        {'name': "pequod",
         'def_part': partfunc,
         'backendcmd': "%s" % (serverCmd),
         'cachecmd': "%s" % (serverCmd),
         'initcmd': "%s" % (initCmd),
         'clientcmd': "%s" % (clientBase)})
    exps.append(exp)
    
    
    # computation/latency experiment. 
    # measure computation times inside the server and see that they are small.
    # should be run on a multiprocessor
    #
    # the enable_validation_logging flag should be set to true and the 
    # logs should be used to determine avg, stddev, and percentiles for
    # the computation times.
    #
    # alternately, the latency for timeline check operations (as measured by the client)
    # can be evaluated by enabling timeline latency logging in the client. 
    # the latency test should really be run on a real network in two setups:
    # 1. with a single cache server
    # 2. with backing servers to demonstrate the added latency for the extra hop
    exp = {'name': "computation", 'defs': []}
    users = "--graph=twitter_graph_1.8M.dat"
    clientBase = "%s %s --no-prevalidate --pactive=70 --duration=1000000000 --checklimit=62795845 " \
                 "--ppost=1 --pread=100 --psubscribe=10 --plogout=5" % \
                 (clientCmd, users)
    
    exp['defs'].append(
        {'name': "pequod",
         'def_part': partfunc,
         'backendcmd': "%s" % (serverCmd),
         'cachecmd': "%s" % (serverCmd),
         'initcmd': "%s" % (initCmd),
         'clientcmd': "%s --popduration=1000000" % (clientBase)})
    exps.append(exp)
    
   
    # cache comparison experiment
    # can be run on a multiprocessor.
    #
    # the number of cache servers pequod uses should be the same as the number of 
    # clients used to access postgres through the DBPool and the same as the
    # number of redis instances.
    # fix %active at 70, post:check ratio at 1:100 and 50 timeline checks per user. 
    exp = {'name': "compare", 'defs': []}
    users = "--graph=twitter_graph_1.8M.dat"
    initBase = "%s --no-binary" % (initCmd)
    clientBase = "%s %s --no-binary --pactive=70 --duration=1000000000 --checklimit=62795845 " \
                 "--ppost=1 --pread=100 --psubscribe=10 --plogout=5" % \
                 (clientCmd, users)
    
    exp['defs'].append(
        {'name': "pequod-client-push",
         'def_part': "twitternew-text",
         'backendcmd': "%s" % (serverCmd),
         'cachecmd': "%s" % (serverCmd),
         'initcmd': "%s --push" % (initBase),
         'clientcmd': "%s --push" % (clientBase)})
    
    exp['defs'].append(
        {'name': "pequod",
         'def_part': "twitternew-text",
         'backendcmd': "%s" % (serverCmd),
         'cachecmd': "%s" % (serverCmd),
         'initcmd': "%s" % (initBase),
         'clientcmd': "%s" % (clientBase)})
    
    exp['defs'].append(
        {'name': "redis",
         'def_redis_compare': True,
         'clientcmd': "%s --push --redis" % (clientBase)})
    
    exp['defs'].append(
        {'name': "memcache",
         'def_single_pop': True,
         'def_memcache_compare': True,
         'def_memcache_args': "-m 61440 -M",
         'clientcmd': "%s --push --memcached" % (clientBase)})
    
    exp['defs'].append(
        {'name': "postgres",
         'def_db_type': "postgres",
         'def_db_sql_script': "scripts/exp/twitter-pg-schema.sql",
         'def_db_in_memory': True,
         'def_db_compare': True,
         'def_db_flags': "-c synchronous_commit=off -c fsync=off " + \
                         "-c full_page_writes=off  -c bgwriter_lru_maxpages=0 " + \
                         "-c shared_buffers=10GB  -c bgwriter_delay=10000 " + \
                         "-c checkpoint_segments=600 ",
         'populatecmd': "%s --initialize --no-execute --popduration=0 --no-binary --dbshim --dbpool-max=10 --dbpool-depth=100 " % (appCmd),
         'clientcmd': "%s --initialize --no-populate --dbshim --dbpool-depth=100" % (clientBase)})
    
    exp['plot'] = {'type': "bar",
                   'data': [{'from': "client",
                             'attr': "wall_time"}],
                   'lines': ["pequod", "pequod-client-push", "redis", "memcache", "postgres"],
                   'ylabel': "Runtime (s)"}
    exps.append(exp)
    
   
    # scale experiment
    # run on a cluster with a ton of memory
    exp = {'name': "scale", 'defs': []}
    users = "--graph=/pequod/twitter_graph_40M.dat"
    
    clientBase = "%s %s --pactive=70 --duration=2000000000 --checklimit=1407239015 " \
                 "--ppost=1 --pread=100 --psubscribe=10 --plogout=5" % \
                 (clientCmd, users)
    
    exp['defs'].append(
        {'name': "pequod",
         'def_part': partfunc,
         'backendcmd': "%s" % (serverCmd),
         'cachecmd': "%s" % (serverCmd),
         'initcmd': "%s" % (initCmd),
         'clientcmd': "%s --no-prevalidate --no-progress-report" % (clientBase)})
    exps.append(exp)


    # cache join comparison
    # compute karma as a single table or interleaved with article data
    # can be run on a multiprocessor
    exp = {'name': "karma", 'defs': []}
    clientBase = "./obj/pqserver --hn --nops=10000000 --large --run_only"
    populateCmd = "./obj/pqserver --hn --narticles=100000 --nusers=50000 --populate_only"
    vote_rate = [0, 1, 10, 25, 50, 75, 100]

    for vr in vote_rate:
        exp['defs'].append(
            {'name': "single_%d" % (vr),
             'def_part': "hackernews",
             'def_single_pop': True,
             'backendcmd': "%s" % (serverCmd),
             'cachecmd': "%s" % (serverCmd),
             'populatecmd' : populateCmd,
             'clientcmd': "%s --vote_rate=%d" % (clientBase, vr)})
        
        exp['defs'].append(
            {'name': "interleaved_%d" % (vr),
             'def_part': "hackernews",
             'def_single_pop': True,
             'backendcmd': "%s" % (serverCmd),
             'cachecmd': "%s" % (serverCmd),
             'populatecmd' : populateCmd + ' --super_materialize',
             'clientcmd': "%s --vote_rate=%d --super_materialize" % (clientBase, vr)})

    exp['plot'] = {'type': "line",
                   'data': [{'from': "client",
                             'attr': "wall_time"}],
                   'lines': ["single", "interleaved"],
                   'points': vote_rate,
                   'xlabel': "Vote Rate (%)",
                   'ylabel': "Runtime (s)"}

    exps.append(exp)

define_experiments()
