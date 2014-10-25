#include <QApplication>
#include "Bingot.h"
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    qDebug() << "test";

    Bingot *bingot = new Bingot();
    bingot->initialize();

    qDebug() << "address:" << bingot->address();
    qDebug() << "privatekey:" << bingot->privateKey();
    qDebug() << "publicKey:" << bingot->publicKey();

    Transaction t(bingot->address(), QString("random_address").toLocal8Bit(), 53);
    t.signTransaction(bingot->privateKey(), bingot->publicKey());
    if (t.verifySignature())
    {
        qDebug() << "signature can be verified";
    }

    qDebug() << "from" << t.getFromAddress();
    qDebug() << "to" << t.getToAddress();

    qDebug() << t.getMessageJson();

    return a.exec();
}
