#include "Bingot.h"
#include <cryptlib.h>
#include <osrng.h>
#include <eccrypto.h>
#include <oids.h>
#include <QDebug>

#include <QCryptographicHash>

const unsigned int protocol_version = 1;

Bingot::Bingot(QObject *parent) :
    QObject(parent),
    m_candidateBlock(0)
{
}

void Bingot::initialize()
{
    generateECDSAKeyPair();
    generateWalletAddress();
}

void Bingot::generateECDSAKeyPair()
{
    CryptoPP::AutoSeededRandomPool autoSeededRandomPool;

    CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PrivateKey privateKey;

    privateKey.Initialize(autoSeededRandomPool, CryptoPP::ASN1::secp160r1());

    bool result = privateKey.Validate( autoSeededRandomPool, 3 );
    if( !result )
    {
        qDebug() << "private key is not valid!";
        return;
    }

    const CryptoPP::Integer &privateKeyInteger = privateKey.GetPrivateExponent();

    QByteArray privateKeyByteArray((int)privateKeyInteger.ByteCount(), 0);
    privateKeyInteger.Encode((byte*)privateKeyByteArray.data(), privateKeyInteger.ByteCount());
    m_privateKey = privateKeyByteArray;

    qDebug() << "private key:" << m_privateKey.size() << m_privateKey.toHex();
    // Generating matching public key
    CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey publicKey;
    privateKey.MakePublicKey(publicKey);

    result = publicKey.Validate( autoSeededRandomPool, 3 );
    if( !result )
    {
        qDebug() << "public key is not valid!";
        return;
    }

    const CryptoPP::ECP::Point& point = publicKey.GetPublicElement();

    QByteArray publicKeyXByteArray((int)point.x.ByteCount(), 0);
    point.x.Encode((byte*)publicKeyXByteArray.data(), point.x.ByteCount());

    QByteArray publicKeyYByteArray((int)point.y.ByteCount(), 0);
    point.y.Encode((byte*)publicKeyYByteArray.data(), point.y.ByteCount());

    m_publicKey.clear();
    m_publicKey.append(publicKeyXByteArray);
    m_publicKey.append(publicKeyYByteArray);

    qDebug() << "public key:" << m_publicKey.size() << m_publicKey.toHex();
}

void Bingot::generateWalletAddress()
{
    QCryptographicHash hasher1(QCryptographicHash::Sha3_512);

    hasher1.addData(m_publicKey);

    QByteArray a1 = hasher1.result();
    a1.prepend((const char*)&protocol_version, sizeof(protocol_version));

    qDebug() << "hash result:" << a1.toHex();

    QCryptographicHash hasher2(QCryptographicHash::Sha256);

    hasher2.addData(a1);
    QByteArray a2 = hasher2.result();

    qDebug() << "hash result:" << a2.toHex();


    QCryptographicHash hasher3(QCryptographicHash::Sha3_256);

    hasher3.addData(a2);

    QByteArray a3 = hasher3.result();

    qDebug() << "cehcksum:" << a3.toHex();

    a1.append(a3.mid(0,4));

    qDebug() << "with checksum" << a1.toHex();

    m_address = a1.toBase64();
    qDebug() << "final address" << m_address;
}

void Bingot::Transfer(const QByteArray &toAddress, unsigned int amount)
{
    Transaction t(address(), toAddress, amount);
    t.signTransaction(privateKey(), publicKey());

    m_suggestedTransactions.insertMulti(t.getSignature(), t);
}

void Bingot::startNewMiningRound()
{
    Block newCandidateBlock(m_blockChain.size(), m_suggestedTransactions, m_blockChain.getLastBlockHash());

    m_candidateBlock = newCandidateBlock;
    m_candidateBlock.addTransaction(Transaction(address()));

    m_suggestedTransactions.clear();

    const unsigned int workerThreadCount = QThread::idealThreadCount();

    quint64 value = Q_UINT64_C(932838457459459);
    quint64 chunk = value/workerThreadCount;

    for(unsigned int i = 0; i < workerThreadCount; ++i)
    {
        Miner *miner = new Miner(m_candidateBlock, chunk*i, chunk*(1+i));
        miner->moveToThread(miner);
        m_miners.push_back(miner);
        miner->start();
    }
}

void Bingot::newBlockReceived(Block b)
{
    if (b.isValid())
    {
        if (b.getIndex() == m_blockChain.size())
        {

           if( m_blockChain.add(b))
           {
//check prehash
            //recycle m_candidateBlock to m_suggestedTransactions;

            //remove m_suggestedTransactions that are in blockChain already

            startNewMiningRound();
           }

        }
        else if(b.getIndex() < m_blockChain.size())
        {
            m_blockChain.add(b);
        }
    }
}

void Bingot::newBlockSolved(Block b)
{
    m_blockChain.add(b);
}
