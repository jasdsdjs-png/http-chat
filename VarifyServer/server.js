const grpc = require('@grpc/grpc-js')
const emailModule = require('./email')
const const_module = require('./const')
const message_proto = require('./proto')
const redis_module = require('./redis')
const { v4: uuidv4 } = require('uuid')

async function GetVarifyCode(call, callback) {
    console.log("email is ", call.request.email)
    try {
        // 每次都生成新验证码，覆盖旧的
        let uniqueId = uuidv4();
        if (uniqueId.length > 4) {
            uniqueId = uniqueId.substring(0, 4);
        }
        let bres = await redis_module.SetRedisExpire(const_module.code_prefix + call.request.email, uniqueId, 300)
        if (!bres) {
            callback(null, {
                email: call.request.email,
                error: const_module.Errors.RedisErr,
                code: ""
            });
            return;
        }

        console.log("uniqueId is ", uniqueId)
        let text_str = '您的验证码为' + uniqueId + '请五分钟内完成注册'
        let mailOptions = {
            from: '1307912807@qq.com',
            to: call.request.email,
            subject: '验证码',
            text: text_str,
        };

        let send_res = await emailModule.SendMail(mailOptions);
        console.log("send res is ", send_res)

        callback(null, {
            email: call.request.email,
            error: const_module.Errors.Success,
            code: uniqueId
        });

    } catch (error) {
        console.log("catch error is ", error)

        callback(null, {
            email: call.request.email,
            error: const_module.Errors.Exception,
            code: ""
        });
    }

}

function main() {
    var server = new grpc.Server()
    server.addService(message_proto.VarifyService.service, { GetVarifyCode: GetVarifyCode })
    server.bindAsync('0.0.0.0:50051', grpc.ServerCredentials.createInsecure(), () => {
        server.start()
        console.log('grpc server started')
    })
}

main()
