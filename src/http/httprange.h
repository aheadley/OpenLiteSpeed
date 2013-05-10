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
#ifndef HTTPRANGE_H
#define HTTPRANGE_H



#include <util/gpointerlist.h>  
  
#define MAX_PART_HEADER_LEN 256

class AutoStr2;
class ByteRange;
class HttpRangeList : public TPointerList<ByteRange>
{
public:
    void clear();
};

class ByteRange;
class HttpRange
{
    HttpRangeList m_list;
    long    m_lEntityLen;
    int     m_iCurRange;
    char    m_boundary[20];
    char    m_partHeaderBuf[MAX_PART_HEADER_LEN];
    char  * m_pPartHeaderEnd;
    char  * m_pCurHeaderPos;
    
    int  checkAndInsert( ByteRange & range );
    void makeBoundaryString();

public: 
    explicit HttpRange( long entityLen = -1 );
    ~HttpRange() {}
    
    int  count() const;
    int  parse( const char * pRange );
    int  getContentRangeString( int n, char * pBuf, int len ) const;
    int  getContentOffset( int n, long& begin, long& end ) const;
    void clear();
    void setContentLen( long entityLen )    { m_lEntityLen = entityLen; }
    void beginMultipart();
    
    const char * getBoundary() const
    {   return m_boundary;  }
    
    long getPartLen( int n, int iMimeTypeLen ) const;
    int  getPartHeader( int n, const char * pMimeType, char* buf, int size ) const;
    int  getPartHeader( const char * pMimeType, char* buf, int size ) const
    {   return getPartHeader( m_iCurRange, pMimeType, buf, size );      }

    long getMultipartBodyLen( const AutoStr2 * pMimeType ) const;
    
    bool more() const;
    void next() {   ++m_iCurRange;  }
    int  getContentOffset( long& begin, long& end ) const;
    int  getPartHeaderLen() const   {   return m_pPartHeaderEnd - m_pCurHeaderPos;  }
    void partHeaderSent( int &len )
    {
        if ( len > m_pPartHeaderEnd - m_pCurHeaderPos )
        {
            len -= m_pPartHeaderEnd - m_pCurHeaderPos;
            m_pCurHeaderPos = m_pPartHeaderEnd;
        }
        else
        {
            m_pCurHeaderPos += len;
            len = 0;
        }
    }       
    int buildPartHeader( const char * pMimeType )
    {   int len = getPartHeader( pMimeType, m_partHeaderBuf, MAX_PART_HEADER_LEN );
        m_pPartHeaderEnd = (char *)m_partHeaderBuf + len;
        m_pCurHeaderPos = m_partHeaderBuf;
        return len;
    }
    const char * getPartHeader() const {    return m_pCurHeaderPos; }
};


#endif
