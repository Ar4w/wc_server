import requests
import json

cont = {"phone":"yourlogin","password":"yourpass","loginMethod":"PERSONAL_OFFICE"}
api_url = "https://lkk.mosobleirc.ru/api/tenants-registration/v2/login"
headers =  {"Content-Type":"application/json"}

response = requests.post(api_url, data=json.dumps(cont), headers=headers)
data = response.json()
if 'token' in data.keys():
    headers['X-Auth-Tenant-Token'] = data['token']
else:
    print( "Login error")
    exit()

api_url = "https://lkk.mosobleirc.ru/api/api/clients"
response = requests.get(api_url, data=json.dumps(cont), headers=headers)
d1 = response.json()
print('*'*100)
print(json.dumps(d1, indent=4, ensure_ascii=False))

api_url = "https://lkk.mosobleirc.ru/api/api/clients/configuration-items"
response = requests.get(api_url, data=json.dumps(cont), headers=headers)
d2 = response.json()
print('*'*100)
print(json.dumps(d2, indent=4, ensure_ascii=False))
print('*'*100)
for i in d2['items']:
    api_url = 'https://lkk.mosobleirc.ru/api/api/clients/meters/for-item/' + str(i['id'])
    response = requests.get(api_url, data=json.dumps(cont), headers=headers)
    d3 = response.json()
    print(json.dumps(d3, indent=4, ensure_ascii=False))
