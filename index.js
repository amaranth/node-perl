var path = require('path');
var fs = require('fs');
var P = require('./build/Release/perl.node');
P.InitPerl();
P.Perl.prototype.lib = function (libDir) {
    if (!libDir || libDir.trim().length === 0) {
        throw new Error("This is not a valid lib directory : " + libDir);
    }
    libDir = path.resolve(process.cwd(), libDir);
    if (!fs.existsSync(libDir)) {
        throw new Error("Requested lib directory does not exist : " + libDir);
    }
    return this.evaluate('use lib qw(' + libDir + ')');
};
P.Perl.prototype.use = function (name) {
    if (!name.match(/^[A-Za-z0-9_:]+$/)) {
        throw new Error("This is not a valid class name : " + name);
    }
    return this.evaluate('use ' + name);
};
module.exports.Perl = P.Perl;
