/*****************************************************************************
*    Open LiteSpeed is an open source HTTP server.                           *
*    Copyright (C) 2013  LiteSpeed Technologies, Inc.                        *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation, either version 3 of the License, or       *
*    (at your option) any later version.                                     *
*                                                                            *
*    This program is distributed in the hope that it will be useful,         *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of          *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the            *
*    GNU General Public License for more details.                            *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see http://www.gnu.org/licenses/.      *
*****************************************************************************/
#include "localworker.h"
#include "localworkerconfig.h"
#include "pidlist.h"

#include <extensions/registry/extappregistry.h>
#include <extensions/cgi/suexec.h>
#include <http/datetime.h>
#include <http/httpglobals.h>
#include <http/httplog.h>
#include <http/httpvhost.h>

#include <main/serverinfo.h>

#include <socket/gsockaddr.h>
#include <util/env.h>
#include <util/ni_fio.h>
#include <util/stringtool.h>

#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define GRACE_TIMEOUT 20
#define KILL_TIMEOUT 25

LocalWorker::LocalWorker()
    : m_fdApp( -1 )
    , m_sigGraceStop( SIGTERM )
    , m_pidList( NULL )
    , m_pidListStop( NULL )
{
    m_pidList = new PidList();
    m_pidListStop = new PidList();
    
}

LocalWorkerConfig& LocalWorker::getConfig() const
{   return *(dynamic_cast<LocalWorkerConfig *>(getConfigPointer()));  }


LocalWorker::~LocalWorker()
{
    if ( m_pidList )
        delete m_pidList;
    if ( m_pidListStop )
        delete m_pidListStop;
}


static void killProcess( pid_t pid )
{
    if ((( kill( pid, SIGTERM ) == -1 )&&( errno == EPERM ))||
        (( kill( pid, SIGUSR1 ) == -1 )&&( errno == EPERM )))
    {
        PidRegistry::markToStop( pid, KILL_TYPE_TERM );
    }    
}


void LocalWorker::moveToStopList()
{
    PidList::iterator iter;
    for( iter = m_pidList->begin(); iter != m_pidList->end();
            iter = m_pidList->next( iter ) )
    {
        m_pidListStop->add( (int)(long)iter->first(), DateTime::s_curTime );
    }
    m_pidList->clear();
}

void LocalWorker::moveToStopList( int pid)
{
    PidList::iterator iter = m_pidList->find( (void *)pid );
    if ( iter != m_pidList->end() )
    {
        killProcess( pid );
        m_pidListStop->add( pid, DateTime::s_curTime - GRACE_TIMEOUT );
        m_pidList->erase( iter );
    }
}

void LocalWorker::cleanStopPids( )
{
    if (( m_pidListStop )&&
        ( m_pidListStop->size() > 0 ))
    {
        pid_t pid;
        PidList::iterator iter, iterDel;
        for( iter = m_pidListStop->begin(); iter != m_pidListStop->end();  )
        {
            pid = (pid_t)(long)iter->first();
            long delta = DateTime::s_curTime - (long)iter->second();
            int sig = 0;
            iterDel = iter;
            iter = m_pidListStop->next( iter );
            if ( delta > GRACE_TIMEOUT )
            {
                if (( kill(  pid, 0 ) == -1 )&&( errno == ESRCH ))
                {
                    m_pidListStop->erase( iterDel );
                    continue;
                }
                if ( delta > KILL_TIMEOUT )
                {
                    sig = SIGKILL;
                    LOG_NOTICE(( "[%s] Send SIGKILL to process [%d] that won't stop.",
                            getName(), pid ));
                }
                else 
                {
                    sig = m_sigGraceStop;
                    LOG_NOTICE(( "[%s] Send SIGTERM to process [%d].",
                            getName(), pid ));
                }
                if ( kill(  pid , sig ) != -1 )
                {
                    if ( D_ENABLED( DL_LESS ) )
                        LOG_D(( "[%s] kill pid: %d", getName(), pid ));
                }
                else if ( errno == EPERM )
                {
                    PidRegistry::markToStop( pid, KILL_TYPE_TERM );
                }
            }
        }
    }
}

void LocalWorker::addPid( pid_t pid )
{
    m_pidList->insert( (void *)(unsigned long )pid, this );
}

void LocalWorker::removePid( pid_t pid)
{
    m_pidList->remove( pid );
    m_pidListStop->remove( pid );
}

int LocalWorker::selfManaged() const
{   return getConfig().getSelfManaged();    }

int LocalWorker::runOnStartUp()
{
    if ( getConfig().getRunOnStartUp() )
        return startEx();
    return 0;
}


int LocalWorker::startOnDemond( int force )
{
    if ( !m_pidList )
        return -1;
    int nProc = m_pidList->size();
    if (( getConfig().getRunOnStartUp() )&&(nProc > 0 ))
        return 0;
    if ( force )
    {
        if ( nProc >= getConfig().getInstances() )
        {
//            if ( getConfig().getInstances() > 1 )
//                return restart();
//            else
                return -1;
        }
    }
    else
    {
        if ( nProc >= getConnPool().getTotalConns() )
            return 0;
        if (( nProc == 0 )&& ( getConnPool().getTotalConns() > 2 ))  //server socket is in use.
            return 0;
    }
    return startEx();
}


int LocalWorker::stop()
{
    pid_t pid;
    PidList::iterator iter;
    removeUnixSocket();
    LOG_NOTICE(( "[%s] stop worker processes", getName() ));
    for( iter = getPidList()->begin(); iter != getPidList()->end();  )
    {
        pid = (pid_t)(long)iter->first();
        iter = getPidList()->next( iter );
        killProcess( pid );
        if ( D_ENABLED( DL_LESS ) )
            LOG_D(( "[%s] kill pid: %d", getName(), pid ));
    }
    moveToStopList();
    setState( ST_NOTSTARTED );

    return 0;
}

void LocalWorker::removeUnixSocket()
{
    const GSockAddr &addr = ((LocalWorkerConfig *)getConfigPointer())->getServerAddr();
    if (( m_fdApp >= 0 )&&( getPidList()->size() > 0 )&&
        ( addr.family() == PF_UNIX ))
    {
        if ( D_ENABLED( DL_LESS ) )
            LOG_D(( "[%s] remove unix socket: %s", getName(),
                addr.getUnix() ));
        unlink( addr.getUnix() );
        close( m_fdApp );
        m_fdApp = -2;
        getConfigPointer()->altServerAddr();        
    }
}


int LocalWorker::addNewProcess()
{
    if (( getConfigPointer()->getURL() )&&
        ( ((LocalWorkerConfig *)getConfigPointer())->getCommand() ))
    {
        return startEx();
    }
    return 1;
}


int LocalWorker::tryRestart()
{
    if ( DateTime::s_curTime - getLastRestart() > 10 )
    {
        LOG_NOTICE(( "[%s] try to fix 503 error by restarting external application", getName() ));
        return restart();
    }
    return 0;
}


int LocalWorker::restart()
{
    setLastRestart( DateTime::s_curTime );
    clearCurConnPool();
    if (( getConfigPointer()->getURL() )&&
        ( ((LocalWorkerConfig *)getConfigPointer())->getCommand() ))
    {
        removeUnixSocket();
        moveToStopList();
        return start();
    }
    return 1;
}



int LocalWorker::getCurInstances() const
{   return getPidList()->size();  }

// void LocalWorker::setPidList( PidList * l)
// {
//     m_pidList = l;
//     if (( l )&& !m_pidListStop )
//         m_pidListStop = new PidList();
// }


static int workerSUExec( LocalWorkerConfig& config, int fd )
{
    const HttpVHost * pVHost = config.getVHost();
    if (( !HttpGlobals::s_pSUExec )||( !pVHost ))
        return -1;
    int mode = pVHost->getRootContext().getSetUidMode();
    if ( mode != UID_DOCROOT )
        return -1;
    uid_t uid = pVHost->getUid();
    gid_t gid = pVHost->getGid();
    if (( uid == HttpGlobals::s_uid )&&
        ( gid == HttpGlobals::s_gid ))
        return -1;
    
    if (( uid < HttpGlobals::s_uidMin )||
        ( gid < HttpGlobals::s_gidMin ))
    {
        LOG_INFO(( "[VHost:%s] Fast CGI [%s]: suExec access denied,"
                    " UID or GID of VHost document root is smaller "
                    "than minimum UID, GID configured. ", pVHost->getName(),
                    config.getName() ));
        return -1;
    }
    const char * pChroot = NULL;
    int chrootLen = 0;
//    if ( HttpGlobals::s_psChroot )
//    {
//        pChroot = HttpGlobals::s_psChroot->c_str();
//        chrootLen = HttpGlobals::s_psChroot->len();
//    }
    char achBuf[4096];
    memccpy( achBuf, config.getCommand(), 0, 4096 );
    char * argv[256];
    char * pDir ;
    SUExec::buildArgv( achBuf, &pDir, argv, 256 );
    if ( pDir )
        *(argv[0]-1) = '/';
    else
        pDir = argv[0];
    HttpGlobals::s_pSUExec->prepare( uid, gid, config.getPriority(),
          pChroot, chrootLen,
          pDir, strlen( pDir ), config.getRLimits() );
    int rfd = -1;
    int pid = HttpGlobals::s_pSUExec->suEXEC( HttpGlobals::s_pServerRoot, &rfd, fd, argv,
                config.getEnv()->get(), NULL );
//    if ( pid != -1)
//    {
//        char achBuf[2048];
//        int ret;
//        while( ( ret = read( rfd, achBuf, 2048 )) > 0 )
//        {
//            write( 2, achBuf, ret );
//        }
//    }
    if ( rfd != -1 )
        close( rfd );

    return pid;
}

int LocalWorker::workerExec( LocalWorkerConfig& config, int fd )
{
    if ( !HttpGlobals::s_pSUExec )
        return -1;
    uid_t uid;
    gid_t gid;
    const HttpVHost * pVHost = config.getVHost();
    if ( !pVHost )
    {
//        if ( config.getRunOnStartUp() == 2 )
//        {
//            uid = 0;
//            gid = 0;
//        }
//        else
        {
            uid = config.getUid();
            gid = config.getGid();
            if ( uid == -1 )
                uid = HttpGlobals::s_uid;
            if  ( gid == -1 )
                gid = HttpGlobals::s_gid;
        }
    }
    else
    {
        int mode = pVHost->getRootContext().getSetUidMode();
        if ( mode != UID_DOCROOT )
            return -1;
        uid = pVHost->getUid();
        gid = pVHost->getGid();
        if ( HttpGlobals::s_ForceGid )
        {
            gid = HttpGlobals::s_ForceGid;
        }
        
        if (( uid < HttpGlobals::s_uidMin )||
            ( gid < HttpGlobals::s_gidMin ))
        {
            if ( D_ENABLED( DL_LESS ) )
                LOG_INFO(( "[VHost:%s] Fast CGI [%s]: suExec access denied,"
                " UID or GID of VHost document root is smaller "
                "than minimum UID, GID configured. ", pVHost->getName(),
                           config.getName() ));
                return -1;
        }
    }
    //if (( uid == HttpGlobals::s_uid )&&
    //    ( gid == HttpGlobals::s_gid ))
    //    return -1;
    const AutoStr2 * pChroot = NULL;
    const char * pChrootPath = NULL;
    int chrootLen = 0;
    int chMode = 0;
    if ( pVHost )
    {
        chMode = pVHost->getRootContext().getChrootMode();
        switch( chMode )
        {
            case CHROOT_VHROOT:
                pChroot = pVHost->getVhRoot();
                break;
            case CHROOT_PATH:
                pChroot = pVHost->getChroot();
                if ( !pChroot->c_str() )
                    pChroot = NULL;
        }
        //Since we already in the chroot jail, do not use the global jail path    
        //If start external app with lscgid, apply global chroot path,
        //  as lscgid is not inside chroot
        if ( config.getStartByServer() == 2 )
        {
            if ( !pChroot )
            {
                pChroot = HttpGlobals::s_psChroot;
            }
        }
        if ( pChroot )
        {
            pChrootPath = pChroot->c_str();
            chrootLen = pChroot->len();
        }
    }
    char achBuf[4096];
    memccpy( achBuf, config.getCommand(), 0, 4096 );
    char * argv[256];
    char * pDir ;
    SUExec::buildArgv( achBuf, &pDir, argv, 256 );
    if ( pDir )
        *(argv[0]-1) = '/';
    else
        pDir = argv[0];
    HttpGlobals::s_pSUExec->prepare( uid, gid, config.getPriority(),
                                     pChrootPath, chrootLen,
                                     pDir, strlen( pDir ), config.getRLimits() );
    int rfd = -1;
    int pid;
    //if ( config.getStartByServer() == 2 )
    //{
        pid = HttpGlobals::s_pSUExec->cgidSuEXEC(
            HttpGlobals::s_pServerRoot, &rfd, fd, argv,
            config.getEnv()->get(), NULL );
    //}
    //else
    //{
    //    pid = HttpGlobals::s_pSUExec->suEXEC(
    //        HttpGlobals::s_pServerRoot, &rfd, fd, argv,
    //        config.getEnv()->get(), NULL );
    //}

    if ( pid == -1 )
        pid = SUExec::spawnChild( config.getCommand(), fd, -1, config.getEnv()->get(),
                    config.getPriority(), config.getRLimits());
    if ( rfd != -1 )
        close( rfd );
        
    return pid;    
}

int LocalWorker::startWorker( )
{
    int fd = getfd();
    LocalWorkerConfig& config = getConfig();
    struct stat st;
//    if (( stat( config.getCommand(), &st ) == -1 )||
//        ( access(config.getCommand(), X_OK) == -1 ))
//    {
//        LOG_ERR(("Start FCGI [%s]: invalid path to executable - %s,"
//                 " not exist or not executable ",
//                config.getName(),config.getCommand() ));
//        return -1;
//    }
//    if ( st.st_mode & S_ISUID )
//    {
//        if ( D_ENABLED( DL_LESS ))
//            LOG_D(( "Fast CGI [%s]: Setuid bit is not allowed : %s\n",
//                config.getName(), config.getCommand() ));
//        return -1;
//    }
    if ( fd < 0 )
    {
        fd = ExtWorker::startServerSock( &config, config.getBackLog() );
        if ( fd != -1 )
        {
            setfd( fd );
            if ( config.getServerAddr().family() == PF_UNIX )
            {
                nio_stat( config.getServerAddr().getUnix(), &st );
                HttpGlobals::getServerInfo()->addUnixSocket(
                     config.getServerAddr().getUnix(), &st );
            }
        }
        else
            return -1;
    }
    int instances = config.getInstances();
    int cur_instances = getCurInstances();
    int new_instances = getConnPool().getTotalConns() + 2 - cur_instances;
    if ( new_instances <= 0 )
        new_instances = 1;
    if ( instances < new_instances + cur_instances )
    {
        new_instances = instances - cur_instances;
    }
    if ( new_instances <= 0 )
        return 0;
    int i;
    for( i = 0; i < new_instances; ++i )
    {
        int pid;
        pid = workerExec( config, fd );
        if ( pid > 0 )
        {
            if ( D_ENABLED( DL_LESS ) )
                LOG_D(( "[%s] add child process pid: %d", getName(), pid ));
           PidRegistry::add( pid, this, 0 );
        }
        else
        {
            LOG_ERR(("Start FCGI [%s]: failed to start the # %d of %d instances.",
                config.getName(), i+1, instances ));
            break;
        }
    }
    return (i==0)?-1:0;
}




