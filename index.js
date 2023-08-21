const addon = require('bindings')('addon');
const { log } = require('console');
const express = require('express');
const path = require('path');
const bodyParser = require('body-parser');

const app = express();

app.set('views', path.join(__dirname, '/public'));
app.set('view engine', 'ejs');

app.use(bodyParser.urlencoded({
    extended: true
}));

const def = { check: false, name: "" };

app.get("/", (req, res) => {
    res.render("index", def);
});

app.post("/", (req, res) => {
    const user = req.body.user;
    if (user) {
        res.render("index", { check: true, name: user, ...addon.get(user) });
    } else {
        res.render("index", def);
    }
});


app.listen(3000, () => {
    console.log("Server start on 3000 port.");
})