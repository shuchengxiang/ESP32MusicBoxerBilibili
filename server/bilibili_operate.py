import requests

BILIBILI_ROOT = 'https://www.bilibili.com'
BILIBILI_API_ROOT = 'https://api.bilibili.com'
HEADERS = {
    'Referer': BILIBILI_ROOT,
    'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
    'Cookie': 'write your own cookie.'
}

def get_video_info(bvid):
    url = BILIBILI_API_ROOT + '/x/web-interface/view?bvid={}'.format(bvid)
    response = requests.get(url, headers=HEADERS, verify=False).json()
    return response.get('data', {})

def get_video_play_url_info(bvid, cid):
    url = BILIBILI_API_ROOT + '/x/player/playurl?bvid={}&cid={}'.format(bvid, cid)
    response = requests.get(url, headers=HEADERS, verify=False).json()
    return response.get('data', {})

def get_play_url_by_bvid(bvid):
    video_info = get_video_info(bvid)
    cid = video_info.get('cid', '')
    play_url_info = get_video_play_url_info(bvid, cid)
    play_url = play_url_info.get('durl', [{}])[0].get('url')
    return play_url

def search_bvid_by_keyword(keyword):
    url = BILIBILI_API_ROOT + '/x/web-interface/wbi/search/all/v2?keyword={}'.format(keyword)
    response = requests.get(url, headers=HEADERS, verify=False).json()
    result = response.get('data', {}).get('result', [])
    if isinstance(result, list) and len(result) > 0:
        for each_result in result:
            result_data = each_result.get('data', [])
            if result_data and len(result_data) > 0:
                bvid = result_data[0].get('bvid', '')
                if bvid:
                    return bvid
    return None

def get_play_url_by_keyword(keyword):
    bvid = search_bvid_by_keyword(keyword)
    if bvid:
        return get_play_url_by_bvid(bvid)

if __name__ == '__main__':
    bvid = search_bvid_by_keyword('早发白帝城 许嵩')
    if bvid:
        print(get_play_url_by_bvid(bvid))

