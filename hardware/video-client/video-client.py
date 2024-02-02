import argparse
import threading
import time
import xml.etree.ElementTree as ET

import requests


class MPD:
    def __init__(self):
        self.periods = []


class Period:
    def __init__(self):
        self.adaptation_sets = []


class AdaptationSet:
    def __init__(self):
        self.mime_type = ""
        self.representations = []


class Representation:
    def __init__(self):
        self.id = ""
        self.height = ""
        self.segment_list = []


lock = threading.Lock()


def get_segment(url):
    start_time = time.time_ns()
    try:
        # resp = requests.get(url)
        resp = requests.get(url, timeout=timeout)
        resp.raise_for_status()
    except requests.exceptions.Timeout:
        lock.acquire()
        print("Error: Request timed out for URL:", url, flush=True)
        lock.release()
    except Exception as err:
        lock.acquire()
        print("Error:", err, flush=True)
        lock.release()
    else:
        end_time = time.time_ns()
        lock.acquire()
        print(end_time - start_time, flush=True)
#        print("Finished getting segment in (ns):", end_time - start_time)
        lock.release()


def get_params():
    parser = argparse.ArgumentParser()
    parser.add_argument("-u", "--url", required=True,
                        help="The URL of the MPD file (should end in '.mpd')")
    parser.add_argument("-r", "--requestInterval", type=int, default=5,
                        help="The interval with which to send request for new segments")
    parser.add_argument("-t", "--timeout", type=int, default=8,
                        help="The time after which the GET request for a segment will time out")
    return parser.parse_args()


def get_mpd(url):
    try:
        resp = requests.get(url)
        resp.raise_for_status()
    except requests.exceptions.RequestException as err:
        raise err

    has_720 = False
    mpd = MPD()
    root = ET.fromstring(resp.content)
    for period in root.findall("{urn:mpeg:dash:schema:mpd:2011}Period"):
        p = Period()
        for adaptation_set in period.findall(
                "{urn:mpeg:dash:schema:mpd:2011}AdaptationSet"):
            a = AdaptationSet()
            a.mime_type = adaptation_set.get("mimeType")
            for representation in adaptation_set.findall(
                    "{urn:mpeg:dash:schema:mpd:2011}Representation"):
                r = Representation()
                r.id = representation.get("id")
                r.height = representation.get("height")
                if r.height == "720":
                    has_720 = True
                for segment_url in representation.findall(
                        "{urn:mpeg:dash:schema:mpd:2011}SegmentList/{urn:mpeg:dash:schema:mpd:2011}SegmentURL"):
                    r.segment_list.append(segment_url.get("media"))
                a.representations.append(r)
            p.adaptation_sets.append(a)
        mpd.periods.append(p)

    if not has_720:
        print("The video does not have a 720p resolution available")
        exit(1)
    return mpd


if __name__ == "__main__":
    args = get_params()

    mpd_url = args.url
    request_interval = args.requestInterval
    timeout = args.timeout

    last_index = mpd_url.rfind("/")
    if last_index == len(mpd_url) - 1:
        mpd_url = mpd_url[:last_index]
        last_index = mpd_url.rfind("/")
    mpd_folder = mpd_url[:last_index + 1]

    MPD = get_mpd(mpd_url)

    for period in MPD.periods:
        assert isinstance(period, Period)
        for adaptation_set in period.adaptation_sets:
            assert isinstance(adaptation_set, AdaptationSet)
            if adaptation_set.mime_type.__contains__("video"):
                for representation in adaptation_set.representations:
                    assert isinstance(representation, Representation)
                    if representation.height != "720":  # Choose 720p video
                        continue
                    for segment_url in representation.segment_list:
                        time.sleep(request_interval)
                        th = threading.Thread(target=get_segment,
                                              args=(mpd_folder + segment_url,))
                        th.start()
                    break  # Do this for the first 720p representation only
                break  # Do this only for the first adaptation set with a video
        break  # Do this only for the first/only period