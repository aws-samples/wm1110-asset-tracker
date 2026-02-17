# Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0


import argparse
import boto3
from botocore.exceptions import ClientError
import json
import logging
import os
import random
import string
from libs.ProvisionWrapper import ProvisionWrapper, InputType, ProvisionWrapperException
from libs.EnvConfig import EnvConfig
from libs.uf2Wrapper import uf2Wrapper, uf2WrapperException

logger = logging.getLogger()
logging.basicConfig(level=logging.INFO)

os.chdir(os.path.dirname(os.path.abspath(__file__)))

ENDPOINT = {
    'prod': 'https://api.iotwireless.us-east-1.amazonaws.com',
}


def ensure_destination_exists(aws_client, destination_name, iam_client):
    """
    Check if a destination exists, and create it if it doesn't.
    Creates an MQTT topic destination for simplicity.
    """
    try:
        response = aws_client.get_destination(Name=destination_name)
        logger.info(f"Destination '{destination_name}' already exists")
        return True
    except ClientError as e:
        if e.response['Error']['Code'] == 'ResourceNotFoundException':
            logger.info(f"Destination '{destination_name}' not found, creating it...")
            return create_destination(aws_client, destination_name, iam_client)
        else:
            raise e


def create_destination(aws_client, destination_name, iam_client):
    """
    Create a new destination that publishes to an MQTT topic.
    """
    # Create IAM role for the destination
    role_name = f"IoTWirelessDestinationRole_{destination_name}"
    role_arn = ensure_destination_role_exists(iam_client, role_name)
    
    if not role_arn:
        logger.error(f"Failed to create/get IAM role for destination '{destination_name}'")
        return False
    
    try:
        # Create destination that publishes to MQTT topic
        # Using MqttTopic expression type to publish to AWS IoT message broker
        mqtt_topic = f"sidewalk/{destination_name}"
        response = aws_client.create_destination(
            Name=destination_name,
            ExpressionType='MqttTopic',
            Expression=mqtt_topic,
            RoleArn=role_arn,
            Description=f"Auto-created destination for {destination_name}"
        )
        logger.info(f"Created destination '{destination_name}' with MQTT topic '{mqtt_topic}'")
        print_response(response)
        return True
    except ClientError as e:
        logger.error(f"Failed to create destination '{destination_name}': {e}")
        return False


def ensure_destination_role_exists(iam_client, role_name):
    """
    Ensure the IAM role for the destination exists, create if it doesn't.
    Returns the role ARN.
    """
    try:
        response = iam_client.get_role(RoleName=role_name)
        logger.info(f"IAM role '{role_name}' already exists")
        return response['Role']['Arn']
    except ClientError as e:
        if e.response['Error']['Code'] == 'NoSuchEntity':
            logger.info(f"IAM role '{role_name}' not found, creating it...")
            return create_destination_role(iam_client, role_name)
        else:
            raise e


def create_destination_role(iam_client, role_name):
    """
    Create an IAM role for the IoT Wireless destination.
    """
    # Trust policy allowing IoT Wireless to assume this role
    trust_policy = {
        "Version": "2012-10-17",
        "Statement": [
            {
                "Effect": "Allow",
                "Principal": {
                    "Service": "iotwireless.amazonaws.com"
                },
                "Action": "sts:AssumeRole"
            }
        ]
    }
    
    # Policy allowing publishing to IoT topics
    iot_policy = {
        "Version": "2012-10-17",
        "Statement": [
            {
                "Effect": "Allow",
                "Action": [
                    "iot:Publish",
                    "iot:DescribeEndpoint"
                ],
                "Resource": "*"
            }
        ]
    }
    
    try:
        # Create the role
        response = iam_client.create_role(
            RoleName=role_name,
            AssumeRolePolicyDocument=json.dumps(trust_policy),
            Description="Role for IoT Wireless destination to publish to MQTT topics"
        )
        role_arn = response['Role']['Arn']
        logger.info(f"Created IAM role '{role_name}'")
        
        # Attach inline policy
        iam_client.put_role_policy(
            RoleName=role_name,
            PolicyName="IoTWirelessPublishPolicy",
            PolicyDocument=json.dumps(iot_policy)
        )
        logger.info(f"Attached IoT publish policy to role '{role_name}'")
        
        # Wait a moment for IAM to propagate
        import time
        logger.info("Waiting for IAM role to propagate...")
        time.sleep(10)
        
        return role_arn
    except ClientError as e:
        logger.error(f"Failed to create IAM role '{role_name}': {e}")
        return None


def main():
    parser = argparse.ArgumentParser(add_help=True)
    parser.add_argument('-i', '--instances', help="Number of instances to generate (default: 1)", required=False,
                        default=1)
    args = parser.parse_args()

    e = EnvConfig("./config.yaml")
    logger.info(f"Address found {e.mfgaddress}")

    aws = AWSHandler(e.env, e.aws_profile)
    iot_client = aws.client
    iam_client = aws.session.client('iam')

    # Ensure uplink destination exists
    logger.info(f"Checking if uplink destination '{e.destination_name}' exists...")
    if not ensure_destination_exists(iot_client, e.destination_name, iam_client):
        logger.error(f"Failed to ensure uplink destination '{e.destination_name}' exists")
        return
    
    # If positioning is enabled, ensure location destination exists
    if e.enable_positioning:
        location_dest = e.location_destination_name if e.location_destination_name else e.destination_name
        if location_dest != e.destination_name:
            logger.info(f"Checking if location destination '{location_dest}' exists...")
            if not ensure_destination_exists(iot_client, location_dest, iam_client):
                logger.error(f"Failed to ensure location destination '{location_dest}' exists")
                return

    if not e.device_profile_id:
        profile_name = 'AssetTracker_prototype_' + ''.join(random.choice(string.ascii_lowercase) for i in range(10))
        logger.info(f"No DeviceProfileID specified. Creating a new DeviceProfile with random name {profile_name}")
        try:
            response = iot_client.create_device_profile(Sidewalk={}, Name=profile_name)
            print_response(response)
            print_status_code(response)
            device_profile_id = response["Id"]
            logger.info(f"Profile created, {device_profile_id}")
            e.update_profile_id(device_profile_id)
        except ClientError as ce:
            if ce.response['Error']['Code'] == 'ConflictException':
                logger.info("Device profile creation conflict - looking for existing Sidewalk device profiles...")
                device_profile_id = find_existing_sidewalk_profile(iot_client)
                if device_profile_id:
                    logger.info(f"Found existing Sidewalk device profile: {device_profile_id}")
                    e.update_profile_id(device_profile_id)
                else:
                    logger.error("No existing Sidewalk device profile found and unable to create new one")
                    return
            else:
                raise ce

    logger.info(f"Getting a DeviceProfile by Id {e.device_profile_id}")
    response = iot_client.get_device_profile(Id=e.device_profile_id)
    print_response(response)
    print_status_code(response)
    del response["ResponseMetadata"]

    logger.info("Saving device profile to file")
    paths = PathsWrapper(e.device_profile_id)
    paths.save_profile_json(response)
    logger.info(f"Saved DeviceProfile details to {paths.get_profile_json_filepath()}")

    failed = False
    for instanceNr in range(0, int(args.instances)):
        logger.info(f"Creating a new WirelessDevice (instance nr {instanceNr})")
        
        # Build the Sidewalk configuration
        sidewalk_config = {"DeviceProfileId": e.device_profile_id}
        
        # Add positioning configuration if enabled
        if e.enable_positioning:
            location_dest = e.location_destination_name if e.location_destination_name else e.destination_name
            sidewalk_config["Positioning"] = {"DestinationName": location_dest}
            logger.info(f"Positioning enabled with location destination: {location_dest}")
        
        # Build the create_wireless_device parameters
        create_params = {
            "Type": "Sidewalk",
            "DestinationName": e.destination_name,
            "Sidewalk": sidewalk_config
        }
        
        # Add positioning flag if enabled
        if e.enable_positioning:
            create_params["Positioning"] = "Enabled"
        
        response = iot_client.create_wireless_device(**create_params)
        print_response(response)
        print_status_code(response)
        wireless_device_id = response["Id"]

        logger.info(f"Getting a WirelessDevice by Id {wireless_device_id}")
        response = iot_client.get_wireless_device(Identifier=wireless_device_id, IdentifierType="WirelessDeviceId")
        print_response(response)
        print_status_code(response)
        del response["ResponseMetadata"]

        logger.info("Saving wireless device to file")
        paths.save_device_json(wireless_device_id, response)

        logger.info("Generating MFG by calling provision.py")
        try:
            p = ProvisionWrapper(script_dir=e.provision_script_directory,
                             silabs_commander_dir=e.commander_dir, hardware_platform=e.hardware_platform)
            p.generate_mfg(wireless_device_path=paths.get_device_json_filepath(wireless_device_id, absPath=True),
                       device_profile_path=paths.get_profile_json_filepath(absPath=True),
                       input_type=InputType.AWS_API_JSONS,
                       output_dir=paths.get_device_dir(wireless_device_id, absPath=True), address=e.mfgaddress)
        except ProvisionWrapperException as pwe:
            logger.error("Failed\n" + str(pwe))
            failed = True
            break

        logger.info("Generating UF2 image (addr=" + e.mfgaddress + ") by calling uf2conv.py")
        try:
            curdir = os.getcwd()
            uf2dir = os.path.abspath(os.path.join(curdir,"tools"))
            u = uf2Wrapper(script_dir=uf2dir)
            u.generate_uf2(hexpath=paths.get_device_dir(wireless_device_id, absPath=True), devicename=wireless_device_id)
        except uf2WrapperException as uwe:
            logger.error("Failed\n" + str(uwe))
            failed = True
            break
        
        if failed:
            logger.error("Unsuccessful")
        else:
            logger.info("Done!")


def make_dir(path):
    try:
        os.mkdir(path)
    except FileExistsError:
        pass
    except OSError as err:
        logger.error(f"An error has occurred: {err}")
        raise


class PathsWrapper:
    def __init__(self, device_profile_id):
        self.device_profile_dir = os.path.join("AssetTrackerProfile_" + device_profile_id)
        make_dir(self.device_profile_dir)

    def get_profile_dir(self):
        return self.device_profile_dir

    def get_profile_json_filepath(self, absPath=False):
        p = os.path.join(self.device_profile_dir, "AssetTrackerProfile.json")
        if absPath:
            return os.path.abspath(p)
        return p

    def save_profile_json(self, data):
        with open(self.get_profile_json_filepath(), 'w') as outfile:
            json.dump(data, outfile, indent=4)

    def get_device_dir(self, device_id, absPath=False):
        p = os.path.join(self.get_profile_dir(), "AssetTracker_" + device_id)
        if absPath:
            return os.path.abspath(p)
        return p

    def get_device_json_filepath(self, device_id, absPath=False):
        p = os.path.join(self.get_device_dir(device_id), "AssetTracker.json")
        if absPath:
            return os.path.abspath(p)
        return p

    def save_device_json(self, device_id, data):
        make_dir(self.get_device_dir(device_id))
        with open(self.get_device_json_filepath(device_id), 'w') as outfile:
            json.dump(data, outfile, indent=4)


class AWSHandler:
    def __init__(self, env, profile_name='default'):
        self.env = env
        self.session = boto3.Session(profile_name=profile_name)
        self.client = self.session.client('iotwireless', endpoint_url=ENDPOINT[env])


def print_response(api_response):
    logger.info(json.dumps(api_response, indent=2))


def get_status_code(api_response):
    code = api_response.get("ResponseMetadata").get("HTTPStatusCode")
    return code


def print_status_code(api_response):
    code = get_status_code(api_response)
    logger.info(f"Status: {code}")


def find_existing_sidewalk_profile(iot_client):
    """
    Search for an existing Sidewalk device profile in the account.
    Returns the first Sidewalk profile ID found, or None if none exist.
    """
    try:
        paginator = iot_client.get_paginator('list_device_profiles')
        for page in paginator.paginate():
            for profile in page.get('DeviceProfileList', []):
                profile_id = profile.get('Id')
                # Get full profile details to check if it's a Sidewalk profile
                try:
                    details = iot_client.get_device_profile(Id=profile_id)
                    if 'Sidewalk' in details:
                        logger.info(f"Found Sidewalk profile: {profile.get('Name', 'unnamed')} ({profile_id})")
                        return profile_id
                except ClientError:
                    continue
        return None
    except ClientError as e:
        logger.error(f"Error listing device profiles: {e}")
        return None


main()
