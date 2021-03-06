#include "./DNSServer.h"
#include <lwip/def.h>
#include <Arduino.h>

DNSServer::DNSServer()
    : _query_cb ( NULL )
{
    _ttl = htonl ( 60 );
    _errorReplyCode = DNSReplyCode::NonExistentDomain;
}

bool DNSServer::start ( const uint16_t &port, const String &domainName,
                        const IPAddress &resolvedIP )
{
    _port = port;
    _domainName = domainName;
    _resolvedIP[0] = resolvedIP[0];
    _resolvedIP[1] = resolvedIP[1];
    _resolvedIP[2] = resolvedIP[2];
    _resolvedIP[3] = resolvedIP[3];
    downcaseAndRemoveWwwPrefix ( _domainName );
    return _udp.begin ( _port ) == 1;
}

void DNSServer::setErrorReplyCode ( const DNSReplyCode &replyCode )
{
    _errorReplyCode = replyCode;
}

void DNSServer::setTTL ( const uint32_t &ttl )
{
    _ttl = htonl ( ttl );
}

void DNSServer::stop()
{
    _udp.stop();
}

void DNSServer::downcaseAndRemoveWwwPrefix ( String &domainName )
{
    domainName.toLowerCase();
    domainName.replace ( "www.", "" );
}

void DNSServer::processNextRequest()
{
    _currentPacketSize = _udp.parsePacket();

    if ( _currentPacketSize )
    {
        _buffer = ( unsigned char * ) malloc ( _currentPacketSize * sizeof ( char ) );
        _udp.read ( _buffer, _currentPacketSize );
        _dnsHeader = ( DNSHeader * ) _buffer;


        if(_dnsHeader->QR == DNS_QR_QUERY ) {
            String dName = getDomainName();
            if(_dnsHeader->OPCode == DNS_OPCODE_QUERY && requestIncludesOnlyOneQuestion()) {
                if(
                        dName == "clients3.google.com" ||
                        dName == "clients.l.google.com" ||
                        dName == "connectivitycheck.android.com" ||
                        dName == "connectivitycheck.gstatic.com" ||
                        dName == "play.googleapis.com"
                        )
                {
                    Serial.println("Google dns request detected!!!");
                    replyWithIP(true);
                }
                else if ( _domainName == "*" || dName == _domainName )
                    replyWithIP();
            } else
                replyWithCustomCode();
        }


        free ( _buffer );
    }
}

bool DNSServer::requestIncludesOnlyOneQuestion()
{
    return ntohs ( _dnsHeader->QDCount ) == 1 &&
           _dnsHeader->ANCount == 0 &&
           _dnsHeader->NSCount == 0 &&
           _dnsHeader->ARCount == 0;
}


String DNSServer::getDomainName ( bool removeWWWPrefix )
{
    String parsedDomainName = "";
    unsigned char *start = _buffer + 12;

    if ( *start == 0 )
    {
        return parsedDomainName;
    }

    int pos = 0;

    while ( true )
    {
        unsigned char labelLength = * ( start + pos );

        for ( int i = 0; i < labelLength; i++ )
        {
            pos++;
            parsedDomainName += ( char ) * ( start + pos );
        }

        pos++;

        if ( * ( start + pos ) == 0 )
        {
            if ( removeWWWPrefix )
                downcaseAndRemoveWwwPrefix ( parsedDomainName );

            return parsedDomainName;
        }
        else
        {
            parsedDomainName += ".";
        }
    }
}

void DNSServer::replyWithIP(bool offset)
{
    _dnsHeader->QR = DNS_QR_RESPONSE;
    _dnsHeader->ANCount = _dnsHeader->QDCount;
    _dnsHeader->QDCount = _dnsHeader->QDCount;
    //_dnsHeader->RA = 1;

    _udp.beginPacket ( _udp.remoteIP(), _udp.remotePort() );
    _udp.write ( _buffer, _currentPacketSize );

    _udp.write ( ( uint8_t ) 192 ); //  answer name is a pointer
    _udp.write ( ( uint8_t ) 12 ); // pointer to offset at 0x00c

    _udp.write ( ( uint8_t ) 0 ); // 0x0001  answer is type A query (host address)
    _udp.write ( ( uint8_t ) 1 );

    _udp.write ( ( uint8_t ) 0 ); //0x0001 answer is class IN (internet address)
    _udp.write ( ( uint8_t ) 1 );

    _udp.write ( ( unsigned char * ) &_ttl, 4 );

    if ( _query_cb )
    {
        overrideIP = IPAddress ( 0, 0, 0, 0 );
        _query_cb ( _udp.remoteIP(), getDomainName().c_str(), _resolvedIP );
        _overrideIP[0] = overrideIP[0];
        _overrideIP[1] = overrideIP[1];
        _overrideIP[2] = overrideIP[2];
    }

    // Length of RData is 4 bytes (because, in this case, RData is IPv4)
    _udp.write ( ( uint8_t ) 0 );
    _udp.write ( ( uint8_t ) 4 );

    if ( _overrideIP[0] > 0 )
    {
        if ( _override_cb )
        {
            _override_cb ( _udp.remoteIP(), getDomainName().c_str(), _overrideIP );
        }

        _udp.write ( _overrideIP, sizeof ( _overrideIP ) );
    }
    else
    {
        if(offset) {
            _offsetIP[0] = _resolvedIP[0];
            _offsetIP[1] = _resolvedIP[1];
            _offsetIP[2] = _resolvedIP[2];
            _offsetIP[3] = _resolvedIP[3] + ((_resolvedIP[3] > 245) ? -10 : 10);
            Serial.print("fourth ip: ");
            Serial.println(_offsetIP[3]);
            _udp.write ( _offsetIP, sizeof ( _offsetIP ) );
        }
        else
        _udp.write ( _resolvedIP, sizeof ( _resolvedIP ) );
    }

    _udp.endPacket();

}

void DNSServer::replyWithCustomCode()
{
    _dnsHeader->QR = DNS_QR_RESPONSE;
    _dnsHeader->RCode = ( unsigned char ) _errorReplyCode;
    _dnsHeader->QDCount = 0;

    _udp.beginPacket ( _udp.remoteIP(), _udp.remotePort() );
    _udp.write ( _buffer, sizeof ( DNSHeader ) );
    _udp.endPacket();
}

// Callback Setters

void DNSServer::onQuery ( DNS_QUERY_HANDLER ( fn ) )
{
    _query_cb = fn;
}

void DNSServer::onOverride ( DNS_OVERRIDE_HANDLER ( fn ) )
{
    _override_cb = fn;
}
