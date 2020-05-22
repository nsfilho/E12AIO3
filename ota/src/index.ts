/* eslint-disable no-console */
import * as fs from 'fs';
import * as path from 'path';
import https from 'https';
import express from 'express';
import morgan from 'morgan';

const PORT = process.env.PORT || 3000;
const app = express();

app.use(morgan('dev'));

app.get('/', (req, res) => {
    const urlFirmware = `https://${req.hostname}:${PORT}/e12aio3.bin`;
    res.send(`Firmware is available at: <a href="${urlFirmware}">${urlFirmware}</a>`);
});

app.get('/e12aio3.bin', (req, res) => {
    res.sendFile(path.join(__dirname, '..', '..', 'build', 'e12aio3.bin'));
});

https
    .createServer(
        {
            key: fs.readFileSync('certs/ca_key.pem'),
            cert: fs.readFileSync('certs/ca_cert.pem'),
        },
        app,
    )
    .listen(PORT, () => {
        const cnString = fs
            .readFileSync('certs/openssl.cnf')
            .toString()
            .split('\n')
            .find((v) => v.startsWith('CN = '));
        if (cnString) {
            const url = `https://${cnString.substr('CN = '.length)}:${PORT}/`;
            console.log('Server is listening on url:', url);
        } else {
            console.log('Server is listening on port:', PORT);
        }
    });
